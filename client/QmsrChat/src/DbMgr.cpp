#include "DbMgr.h"
#include <QDebug>
#include <QSqlRecord>

DbMgr& DbMgr::Instance()
{
    static DbMgr instance;
    return instance;
}

bool DbMgr::Init(const QString& db_path)
{
    QMutexLocker lock(&_mutex);
    
    _connection_name = QString("chat_db_%1").arg(reinterpret_cast<quintptr>(this));
    _db = QSqlDatabase::addDatabase("QSQLITE", _connection_name);
    _db.setDatabaseName(db_path);
    
    if (!_db.open()) {
        qDebug() << "Failed to open database:" << _db.lastError().text();
        return false;
    }
    
    if (!CreateTables()) {
        qDebug() << "Failed to create tables:" << _db.lastError().text();
        return false;
    }
    
    return true;
}

void DbMgr::Shutdown()
{
    QMutexLocker lock(&_mutex);
    
    if (_db.isOpen()) {
        _db.close();
    }
    
    QSqlDatabase::removeDatabase(_connection_name);
}

bool DbMgr::CreateTables()
{
    QSqlQuery query(_db);
    
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            status INTEGER DEFAULT 0,
            UNIQUE(from_uid, to_uid, timestamp)
        )
    )";
    
    if (!query.exec(sql)) {
        qDebug() << "Failed to create messages table:" << query.lastError().text();
        return false;
    }
    
    sql = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_search 
        ON messages(from_uid, to_uid, timestamp DESC)
    )";
    
    if (!query.exec(sql)) {
        qDebug() << "Failed to create messages index:" << query.lastError().text();
        return false;
    }
    
    return true;
}

bool DbMgr::SaveMessage(const ChatMessage& msg)
{
    QMutexLocker lock(&_mutex);
    
    QSqlQuery query(_db);
    query.prepare("INSERT OR REPLACE INTO messages (from_uid, to_uid, content, timestamp, status) VALUES (?, ?, ?, ?, ?)");
    
    query.bindValue(0, msg.from_uid);
    query.bindValue(1, msg.to_uid);
    query.bindValue(2, msg.content);
    query.bindValue(3, msg.timestamp);
    query.bindValue(4, msg.status);
    
    if (!query.exec()) {
        qDebug() << "Failed to save message:" << query.lastError().text();
        return false;
    }
    
    return true;
}

QVector<ChatMessage> DbMgr::GetMessages(int uid1, int uid2, qint64 before_time, int limit)
{
    QMutexLocker lock(&_mutex);
    
    QVector<ChatMessage> messages;
    QSqlQuery query(_db);
    
    QString sql = R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM messages 
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND timestamp < ?
        ORDER BY timestamp DESC 
        LIMIT ?
    )";
    
    query.prepare(sql);
    query.bindValue(0, uid1);
    query.bindValue(1, uid2);
    query.bindValue(2, uid2);
    query.bindValue(3, uid1);
    query.bindValue(4, before_time);
    query.bindValue(5, limit);
    
    if (!query.exec()) {
        qDebug() << "Failed to get messages:" << query.lastError().text();
        return messages;
    }
    
    while (query.next()) {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();
        msg.from_uid = query.value(1).toInt();
        msg.to_uid = query.value(2).toInt();
        msg.content = query.value(3).toString();
        msg.timestamp = query.value(4).toLongLong();
        msg.status = query.value(5).toInt();
        messages.push_back(msg);
    }
    
    return messages;
}

QVector<ChatMessage> DbMgr::SearchMessages(int uid1, int uid2, const QString& keyword, int limit)
{
    QMutexLocker lock(&_mutex);
    
    QVector<ChatMessage> messages;
    QSqlQuery query(_db);
    
    QString sql = R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM messages 
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND content LIKE ?
        ORDER BY timestamp DESC 
        LIMIT ?
    )";
    
    query.prepare(sql);
    query.bindValue(0, uid1);
    query.bindValue(1, uid2);
    query.bindValue(2, uid2);
    query.bindValue(3, uid1);
    query.bindValue(4, QString("%%1%").arg(keyword));
    query.bindValue(5, limit);
    
    if (!query.exec()) {
        qDebug() << "Failed to search messages:" << query.lastError().text();
        return messages;
    }
    
    while (query.next()) {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();
        msg.from_uid = query.value(1).toInt();
        msg.to_uid = query.value(2).toInt();
        msg.content = query.value(3).toString();
        msg.timestamp = query.value(4).toLongLong();
        msg.status = query.value(5).toInt();
        messages.push_back(msg);
    }
    
    return messages;
}

bool DbMgr::DeleteMessages(int uid1, int uid2)
{
    QMutexLocker lock(&_mutex);
    
    QSqlQuery query(_db);
    query.prepare("DELETE FROM messages WHERE (from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?)");
    
    query.bindValue(0, uid1);
    query.bindValue(1, uid2);
    query.bindValue(2, uid2);
    query.bindValue(3, uid1);
    
    if (!query.exec()) {
        qDebug() << "Failed to delete messages:" << query.lastError().text();
        return false;
    }
    
    return true;
}

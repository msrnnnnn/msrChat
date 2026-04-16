#include "DbMgr.h"
#include <QDebug>
#include <QSqlRecord>
#include <QCoreApplication>

QThreadStorage<QSqlDatabase> g_thread_db_cache;

DbMgr::DbMgr()
    : _initialized(false)
{
}

DbMgr::~DbMgr()
{
    Destroy();
}

DbMgr& DbMgr::Instance()
{
    static DbMgr instance;
    return instance;
}

bool DbMgr::Init(const QString& db_path)
{
    QMutexLocker lock(&_init_mutex);
    
    if (_initialized) {
        qDebug() << "DbMgr already initialized";
        return true;
    }
    
    _db_path = db_path;
    
    _main_thread_connection_name = QString("chat_db_main_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    _main_thread_db = QSqlDatabase::addDatabase("QSQLITE", _main_thread_connection_name);
    _main_thread_db.setDatabaseName(db_path);
    
    if (!_main_thread_db.open()) {
        qDebug() << "Failed to open main thread database:" << _main_thread_db.lastError().text();
        return false;
    }
    
    if (!CreateTables(_main_thread_db)) {
        qDebug() << "Failed to create tables in main thread database:" << _main_thread_db.lastError().text();
        return false;
    }
    
    g_thread_db_cache.setLocalData(_main_thread_db);
    
    _initialized = true;
    qDebug() << "DbMgr initialized successfully";
    qDebug() << "  - Main thread connection:" << _main_thread_connection_name;
    
    return true;
}

void DbMgr::Destroy()
{
    QMutexLocker lock(&_init_mutex);
    
    if (!_initialized) {
        return;
    }
    
    CloseAllThreadConnections();
    
    if (_main_thread_db.isOpen()) {
        _main_thread_db.close();
    }
    QSqlDatabase::removeDatabase(_main_thread_connection_name);
    
    _initialized = false;
    qDebug() << "DbMgr destroyed and all connections closed";
}

QSqlDatabase& DbMgr::GetOrCreateThreadConnection()
{
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        return _main_thread_db;
    }
    
    if (g_thread_db_cache.hasLocalData()) {
        return g_thread_db_cache.localData();
    }
    
    QString conn_name = QString("chat_db_worker_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
    db.setDatabaseName(_db_path);
    
    if (!db.open()) {
        qDebug() << "Failed to open database for worker thread:" << db.lastError().text();
        static QSqlDatabase null_db;
        return null_db;
    }
    
    g_thread_db_cache.setLocalData(db);
    qDebug() << "Created new database connection for thread:" << QThread::currentThread();
    
    return g_thread_db_cache.localData();
}

void DbMgr::CloseAllThreadConnections()
{
}

bool DbMgr::CreateTables(QSqlDatabase& db)
{
    QSqlQuery query(db);
    
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
    QSqlDatabase& db = GetOrCreateThreadConnection();
    if (!db.isOpen()) {
        qDebug() << "Database not open in SaveMessage";
        return false;
    }
    
    QSqlQuery query(db);
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
    QVector<ChatMessage> messages;
    
    QSqlDatabase& db = GetOrCreateThreadConnection();
    if (!db.isOpen()) {
        qDebug() << "Database not open in GetMessages";
        return messages;
    }
    
    QSqlQuery query(db);
    
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
    QVector<ChatMessage> messages;
    
    QSqlDatabase& db = GetOrCreateThreadConnection();
    if (!db.isOpen()) {
        qDebug() << "Database not open in SearchMessages";
        return messages;
    }
    
    QSqlQuery query(db);
    
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
    query.bindValue(4, QString("%%").append(keyword).append("%%"));
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
    QSqlDatabase& db = GetOrCreateThreadConnection();
    if (!db.isOpen()) {
        qDebug() << "Database not open in DeleteMessages";
        return false;
    }
    
    QSqlQuery query(db);
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

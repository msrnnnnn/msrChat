#include "DbMgr.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSqlRecord>

QThreadStorage<QSqlDatabase> g_thread_db_cache;

DbMgr::DbMgr()
    : _initialized(false)
{
}

DbMgr::~DbMgr()
{
    Destroy();
}

DbMgr &DbMgr::Instance()
{
    static DbMgr instance;
    return instance;
}

bool DbMgr::Init(const QString &db_path)
{
    QMutexLocker lock(&_init_mutex);

    if (_initialized)
    {
        qDebug() << "DbMgr already initialized";
        return true;
    }

    _db_path = db_path;

    _main_thread_connection_name =
        QString("chat_db_main_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    _main_thread_db = QSqlDatabase::addDatabase("QSQLITE", _main_thread_connection_name);
    _main_thread_db.setDatabaseName(db_path);

    if (!_main_thread_db.open())
    {
        qDebug() << "Failed to open main thread database:" << _main_thread_db.lastError().text();
        return false;
    }

    if (!CreateTables(_main_thread_db))
    {
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
    QMutexLocker lock(&Instance()._init_mutex);

    if (!Instance()._initialized)
    {
        return;
    }

    Instance().CloseAllThreadConnections();

    if (Instance()._main_thread_db.isOpen())
    {
        Instance()._main_thread_db.close();
    }
    QSqlDatabase::removeDatabase(Instance()._main_thread_connection_name);

    Instance()._initialized = false;
    qDebug() << "DbMgr destroyed and all connections closed";
}

QSqlDatabase &DbMgr::GetOrCreateThreadConnection()
{
    if (QThread::currentThread() == QCoreApplication::instance()->thread())
    {
        return _main_thread_db;
    }

    if (g_thread_db_cache.hasLocalData())
    {
        return g_thread_db_cache.localData();
    }

    QString conn_name = QString("chat_db_worker_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
    db.setDatabaseName(_db_path);

    if (!db.open())
    {
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

bool DbMgr::CreateTables(QSqlDatabase &db)
{
    QSqlQuery query(db);

    QString sql = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            client_msg_id TEXT DEFAULT '',
            server_msg_id INTEGER DEFAULT 0,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            status INTEGER DEFAULT 0
        )
    )";

    if (!query.exec(sql))
    {
        qDebug() << "Failed to create messages table:" << query.lastError().text();
        return false;
    }

    if (!EnsureColumn(db, "messages", "client_msg_id", "TEXT DEFAULT ''"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "server_msg_id", "INTEGER DEFAULT 0"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "status", "INTEGER DEFAULT 0"))
    {
        return false;
    }

    sql = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_pair_time
        ON messages(from_uid, to_uid, timestamp DESC)
    )";

    if (!query.exec(sql))
    {
        qDebug() << "Failed to create messages index:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_messages_client_msg_id ON messages(client_msg_id)"))
    {
        qDebug() << "Failed to create client_msg_id index:" << query.lastError().text();
        return false;
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_messages_server_msg_id ON messages(server_msg_id)"))
    {
        qDebug() << "Failed to create server_msg_id index:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DbMgr::EnsureColumn(QSqlDatabase &db, const QString &table, const QString &column, const QString &definition)
{
    const QSqlRecord record = db.record(table);
    if (record.indexOf(column) >= 0)
    {
        return true;
    }

    QSqlQuery query(db);
    const QString sql = QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, definition);
    if (!query.exec(sql))
    {
        qDebug() << "Failed to add column" << column << ":" << query.lastError().text();
        return false;
    }
    return true;
}

qint64 DbMgr::FindMessageId(QSqlDatabase &db, const ChatMessage &msg)
{
    QSqlQuery query(db);

    if (!msg.client_msg_id.isEmpty())
    {
        query.prepare("SELECT id FROM messages WHERE client_msg_id = ? LIMIT 1");
        query.bindValue(0, msg.client_msg_id);
        if (query.exec() && query.next())
        {
            return query.value(0).toLongLong();
        }
    }

    if (msg.server_msg_id > 0)
    {
        query.prepare("SELECT id FROM messages WHERE server_msg_id = ? LIMIT 1");
        query.bindValue(0, msg.server_msg_id);
        if (query.exec() && query.next())
        {
            return query.value(0).toLongLong();
        }
    }

    query.prepare(
        "SELECT id FROM messages WHERE from_uid = ? AND to_uid = ? AND timestamp = ? AND content = ? LIMIT 1");
    query.bindValue(0, msg.from_uid);
    query.bindValue(1, msg.to_uid);
    query.bindValue(2, msg.timestamp);
    query.bindValue(3, msg.content);
    if (query.exec() && query.next())
    {
        return query.value(0).toLongLong();
    }

    return 0;
}

bool DbMgr::SaveMessage(const ChatMessage &msg)
{
    QSqlDatabase &db = GetOrCreateThreadConnection();
    if (!db.isOpen())
    {
        qDebug() << "Database not open in SaveMessage";
        return false;
    }

    const qint64 existingId = FindMessageId(db, msg);
    QSqlQuery query(db);

    if (existingId > 0)
    {
        query.prepare(
            "UPDATE messages "
            "SET client_msg_id = ?, server_msg_id = ?, from_uid = ?, to_uid = ?, content = ?, timestamp = ?, status = "
            "? "
            "WHERE id = ?");
        query.bindValue(7, existingId);
    }
    else
    {
        query.prepare(
            "INSERT INTO messages (client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
    }

    query.bindValue(0, msg.client_msg_id);
    query.bindValue(1, msg.server_msg_id);
    query.bindValue(2, msg.from_uid);
    query.bindValue(3, msg.to_uid);
    query.bindValue(4, msg.content);
    query.bindValue(5, msg.timestamp);
    query.bindValue(6, msg.status);

    if (!query.exec())
    {
        qDebug() << "Failed to save message:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DbMgr::UpdateMessageStatus(const QString &client_msg_id, int status)
{
    if (client_msg_id.isEmpty())
    {
        return false;
    }

    QSqlDatabase &db = GetOrCreateThreadConnection();
    if (!db.isOpen())
    {
        qDebug() << "Database not open in UpdateMessageStatus";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE messages SET status = ? WHERE client_msg_id = ?");
    query.bindValue(0, status);
    query.bindValue(1, client_msg_id);

    if (!query.exec())
    {
        qDebug() << "Failed to update message status:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QVector<ChatMessage> DbMgr::GetMessages(int uid1, int uid2, qint64 before_time, int limit)
{
    QVector<ChatMessage> messages;

    QSqlDatabase &db = GetOrCreateThreadConnection();
    if (!db.isOpen())
    {
        qDebug() << "Database not open in GetMessages";
        return messages;
    }

    QSqlQuery query(db);

    QString sql = R"(
        SELECT id, client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status
        FROM (
            SELECT id, client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status
            FROM messages
            WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
            AND timestamp < ?
            ORDER BY timestamp DESC
            LIMIT ?
        )
        ORDER BY timestamp ASC
    )";

    query.prepare(sql);
    query.bindValue(0, uid1);
    query.bindValue(1, uid2);
    query.bindValue(2, uid2);
    query.bindValue(3, uid1);
    query.bindValue(4, before_time);
    query.bindValue(5, limit);

    if (!query.exec())
    {
        qDebug() << "Failed to get messages:" << query.lastError().text();
        return messages;
    }

    while (query.next())
    {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();
        msg.client_msg_id = query.value(1).toString();
        msg.server_msg_id = query.value(2).toLongLong();
        msg.from_uid = query.value(3).toInt();
        msg.to_uid = query.value(4).toInt();
        msg.content = query.value(5).toString();
        msg.timestamp = query.value(6).toLongLong();
        msg.status = query.value(7).toInt();
        messages.push_back(msg);
    }

    return messages;
}

QVector<ChatMessage> DbMgr::SearchMessages(int uid1, int uid2, const QString &keyword, int limit)
{
    QVector<ChatMessage> messages;

    QSqlDatabase &db = GetOrCreateThreadConnection();
    if (!db.isOpen())
    {
        qDebug() << "Database not open in SearchMessages";
        return messages;
    }

    QSqlQuery query(db);

    QString sql = R"(
        SELECT id, client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status
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

    if (!query.exec())
    {
        qDebug() << "Failed to search messages:" << query.lastError().text();
        return messages;
    }

    while (query.next())
    {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();
        msg.client_msg_id = query.value(1).toString();
        msg.server_msg_id = query.value(2).toLongLong();
        msg.from_uid = query.value(3).toInt();
        msg.to_uid = query.value(4).toInt();
        msg.content = query.value(5).toString();
        msg.timestamp = query.value(6).toLongLong();
        msg.status = query.value(7).toInt();
        messages.push_back(msg);
    }

    return messages;
}

bool DbMgr::DeleteMessages(int uid1, int uid2)
{
    QSqlDatabase &db = GetOrCreateThreadConnection();
    if (!db.isOpen())
    {
        qDebug() << "Database not open in DeleteMessages";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM messages WHERE (from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?)");
    query.bindValue(0, uid1);
    query.bindValue(1, uid2);
    query.bindValue(2, uid2);
    query.bindValue(3, uid1);

    if (!query.exec())
    {
        qDebug() << "Failed to delete messages:" << query.lastError().text();
        return false;
    }

    return true;
}

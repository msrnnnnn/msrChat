#include "DbService.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSqlRecord>

QThreadStorage<QSqlDatabase> g_thread_db_cache;

DbService::DbService()
    : _initialized(false)
{
}

DbService::~DbService()
{
    Destroy();
}

DbService &DbService::Instance()
{
    static DbService instance;
    return instance;
}

/**
 * @brief 初始化数据库
 * @param db_path 数据库文件路径
 * @return 是否初始化成功
 * @details 主线程创建连接和表，工作线程按需创建连接
 */
bool DbService::Init(const QString &db_path)
{
    QMutexLocker lock(&_init_mutex);

    if (_initialized)
    {
        qDebug() << "DbService already initialized";
        return true;
    }

    _db_path = db_path;

    _main_thread_connection_name = QString("chat_db_main_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
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
    qDebug() << "DbService initialized successfully";
    qDebug() << "  - Main thread connection:" << _main_thread_connection_name;

    return true;
}

/**
 * @brief 销毁数据库管理器
 * @details 关闭所有线程连接，移除主线程数据库连接
 */
void DbService::Destroy()
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
    qDebug() << "DbService destroyed and all connections closed";
}

/**
 * @brief 获取或创建当前线程的数据库连接
 * @return 数据库连接引用
 * @details 主线程返回主连接，工作线程使用 QThreadStorage 缓存独立连接
 */
QSqlDatabase &DbService::GetOrCreateThreadConnection()
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

void DbService::CloseAllThreadConnections()
{
    // QThreadStorage 会在每个线程退出时自动析构 QSqlDatabase 并关闭连接，
    // 因此此处不需要手动遍历关闭。当前只有工作线程会通过 GetOrCreateThreadConnection()
    // 创建独立连接，这些连接随线程退出由 QThreadStorage 自动回收。
}

/**
 * @brief 创建数据库表结构
 * @param db 数据库连接
 * @return 是否创建成功
 * @details 包括 messages 表及多个索引
 */
bool DbService::CreateTables(QSqlDatabase &db)
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
    if (!EnsureColumn(db, "messages", "type", "INTEGER DEFAULT 0"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "image_id", "TEXT"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "image_path", "TEXT"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "image_width", "INTEGER DEFAULT 0"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "image_height", "INTEGER DEFAULT 0"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "image_ext", "TEXT"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "edited", "INTEGER DEFAULT 0"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "edited_at", "INTEGER"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "recalled", "INTEGER DEFAULT 0"))
    {
        return false;
    }
    if (!EnsureColumn(db, "messages", "recalled_at", "INTEGER"))
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

/**
 * @brief 确保表存在指定列
 * @param db 数据库连接
 * @param table 表名
 * @param column 列名
 * @param definition 列定义（如 "TEXT DEFAULT ''"）
 * @return 列是否存在或创建成功
 */
bool DbService::EnsureColumn(QSqlDatabase &db, const QString &table, const QString &column, const QString &definition)
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

/**
 * @brief 根据消息特征查找消息 ID
 * @param db 数据库连接
 * @param msg 消息结构体
 * @return 消息 ID（未找到返回 0）
 * @details 优先用 client_msg_id，其次 server_msg_id，最后用 (from_uid, to_uid, timestamp, content) 组合
 */
qint64 DbService::FindMessageId(QSqlDatabase &db, const ChatMessage &msg)
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

/**
 * @brief 保存消息（插入或更新）
 * @param msg 消息结构体
 * @return 是否保存成功
 * @details 根据 client_msg_id 或 server_msg_id 判断是否已存在，实现幂等 upsert
 */
bool DbService::SaveMessage(const ChatMessage &msg)
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
            "?, type = ?, image_id = ?, image_path = ?, image_width = ?, image_height = ?, image_ext = ?, edited = ?, edited_at = ?, recalled = ?, recalled_at = ? "
            "WHERE id = ?");
        query.bindValue(17, existingId);
    }
    else
    {
        query.prepare(
            "INSERT INTO messages (client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status, type, image_id, image_path, image_width, image_height, image_ext, edited, edited_at, recalled, recalled_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    }

    query.bindValue(0, msg.client_msg_id);
    query.bindValue(1, msg.server_msg_id);
    query.bindValue(2, msg.from_uid);
    query.bindValue(3, msg.to_uid);
    query.bindValue(4, msg.content);
    query.bindValue(5, msg.timestamp);
    query.bindValue(6, msg.status);
    query.bindValue(7, msg.type);
    query.bindValue(8, msg.image_id);
    query.bindValue(9, msg.image_path);
    query.bindValue(10, msg.image_width);
    query.bindValue(11, msg.image_height);
    query.bindValue(12, msg.image_ext);
    query.bindValue(13, msg.edited ? 1 : 0);
    query.bindValue(14, msg.edited_at);
    query.bindValue(15, msg.recalled ? 1 : 0);
    query.bindValue(16, msg.recalled_at);

    if (!query.exec())
    {
        qDebug() << "Failed to save message:" << query.lastError().text();
        return false;
    }

    return true;
}

/**
 * @brief 更新消息状态
 * @param client_msg_id 客户端消息 ID
 * @param status 新状态（0=发送中，1=已送达，2=已存储，-1=失败）
 * @return 是否更新成功
 */
bool DbService::UpdateMessageStatus(const QString &client_msg_id, int status)
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

/**
 * @brief 获取两个用户之间的聊天历史
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @param before_time 时间戳上限（默认 LLONG_MAX）
 * @param limit 每页消息数（默认 50）
 * @return 消息列表（按时间升序）
 */
QVector<ChatMessage> DbService::GetMessages(int uid1, int uid2, qint64 before_time, int limit)
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
        SELECT id, client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status,
               type, image_id, image_path, image_width, image_height, image_ext,
               edited, edited_at, recalled, recalled_at
        FROM (
            SELECT id, client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status,
                   type, image_id, image_path, image_width, image_height, image_ext,
                   edited, edited_at, recalled, recalled_at
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
        msg.type = query.value(8).toInt();
        msg.image_id = query.value(9).toString();
        msg.image_path = query.value(10).toString();
        msg.image_width = query.value(11).toInt();
        msg.image_height = query.value(12).toInt();
        msg.image_ext = query.value(13).toString();
        msg.edited = query.value(14).toBool();
        msg.edited_at = query.value(15).toLongLong();
        msg.recalled = query.value(16).toBool();
        msg.recalled_at = query.value(17).toLongLong();
        messages.push_back(msg);
    }

    return messages;
}

/**
 * @brief 搜索两个用户之间的消息
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @param keyword 关键词（LIKE 模糊匹配）
 * @param limit 返回条数限制
 * @return 匹配的消息列表
 */
QVector<ChatMessage> DbService::SearchMessages(int uid1, int uid2, const QString &keyword, int limit)
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
        FROM (
            SELECT id, client_msg_id, server_msg_id, from_uid, to_uid, content, timestamp, status
            FROM messages 
            WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
            AND content LIKE ?
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
        msg.type = query.value(8).toInt();
        msg.image_id = query.value(9).toString();
        msg.image_path = query.value(10).toString();
        msg.image_width = query.value(11).toInt();
        msg.image_height = query.value(12).toInt();
        msg.image_ext = query.value(13).toString();
        msg.edited = query.value(14).toBool();
        msg.edited_at = query.value(15).toLongLong();
        msg.recalled = query.value(16).toBool();
        msg.recalled_at = query.value(17).toLongLong();
        messages.push_back(msg);
    }

    return messages;
}

/**
 * @brief 删除两个用户之间的所有聊天记录
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @return 是否删除成功
 */
bool DbService::DeleteMessages(int uid1, int uid2)
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

/**
 * @brief 按时间戳删除单条消息（Phase 6）
 * @param ts 消息时间戳（毫秒）
 * @return 是否删除成功
 */
bool DbService::DeleteMessageByTimestamp(qint64 ts)
{
    QSqlDatabase &db = GetOrCreateThreadConnection();
    if (!db.isOpen())
    {
        qDebug() << "Database not open in DeleteMessageByTimestamp";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM messages WHERE timestamp = ?");
    query.bindValue(0, ts);

    if (!query.exec())
    {
        qDebug() << "Failed to delete message by timestamp:" << query.lastError().text();
        return false;
    }

    return true;
}

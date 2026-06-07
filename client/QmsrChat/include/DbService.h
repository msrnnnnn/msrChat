/**
 * @file DbService.h
 * @brief 本地 SQLite 数据库服务
 * @details 单例模式，负责聊天消息的持久化存储，支持多线程并发访问，通过线程本地连接隔离数据库操作。
 */
#ifndef DBSERVICE_H
#define DBSERVICE_H

#include <QMetaType>
#include <QMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QThread>
#include <QThreadStorage>
#include <QVector>

/**
 * @brief 聊天消息数据实体
 * @details 涵盖文本消息和图片消息，支持撤回和编辑标记。
 */
struct ChatMessage
{
    qint64 id = 0;
    QString client_msg_id;
    qint64 server_msg_id = 0;
    int from_uid = 0;
    int to_uid = 0;
    QString content;
    qint64 timestamp = 0;
    int status = 0;
    // === image + recall + edit (Phase 3 新增) ===
    int     type        = 0;     // 0=text, 1=image
    QString image_id;           // UUID
    QString image_path;         // 本地缓存绝对路径
    int     image_width = 0;
    int     image_height = 0;
    QString image_ext;
    bool    edited      = false;
    qint64  edited_at   = 0;
    bool    recalled    = false;
    qint64  recalled_at = 0;
};

Q_DECLARE_METATYPE(ChatMessage)
Q_DECLARE_METATYPE(QVector<ChatMessage>)

/**
 * @brief 本地 SQLite 数据库操作单例
 * @details 管理主线程数据库连接，支持多线程通过 QThreadStorage 获取各自的数据库连接，提供消息增删改查和结构迁移功能。
 */
class DbService
{
public:
    static DbService &Instance();

    /**
     * @brief 初始化数据库，创建表结构，执行结构迁移
     * @param db_path SQLite 数据库文件路径
     * @return 初始化是否成功
     */
    bool Init(const QString &db_path);
    static void Destroy();

    bool SaveMessage(const ChatMessage &msg);
    bool UpdateMessageStatus(const QString &client_msg_id, int status);
    bool UpdateImagePath(const QString &image_id, const QString &local_path);
    /**
     * @brief 按会话双方 uid 获取历史消息
     * @param before_time 分页锚点，返回此时间之前的消息（默认取最早）
     * @param limit 每页条数
     */
    QVector<ChatMessage> GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    /**
     * @brief 在双方会话中全文搜索消息内容
     */
    QVector<ChatMessage> SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    /**
     * @brief 删除双方之间的全部消息
     */
    bool DeleteMessages(int uid1, int uid2);
    /**
     * @brief 按时间戳删除单条消息（Phase 6）
     */
    bool DeleteMessageByTimestamp(qint64 ts);
    /**
     * @brief 将消息标记为已撤回持久化
     */
    bool MarkMessageRecalled(qint64 ts, int current_uid);

    DbService(const DbService &) = delete;
    DbService &operator=(const DbService &) = delete;

private:
    DbService();
    ~DbService();

    bool CreateTables(QSqlDatabase &db);
    /**
     * @brief 检查表中列是否存在，不存在则添加（结构迁移）
     */
    bool EnsureColumn(QSqlDatabase &db, const QString &table, const QString &column, const QString &definition);
    /**
     * @brief 根据消息关键字段查找已存在的消息 ID（去重）
     */
    qint64 FindMessageId(QSqlDatabase &db, const ChatMessage &msg);
    /**
     * @brief 为当前线程获取或创建独立的数据库连接
     */
    QSqlDatabase &GetOrCreateThreadConnection();
    void CloseAllThreadConnections();

    QSqlDatabase _main_thread_db;
    QString _main_thread_connection_name;
    QString _db_path;
    bool _initialized;
    QMutex _init_mutex;
};

#endif

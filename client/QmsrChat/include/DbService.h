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

class DbService
{
public:
    static DbService &Instance();

    bool Init(const QString &db_path);
    static void Destroy();

    bool SaveMessage(const ChatMessage &msg);
    bool UpdateMessageStatus(const QString &client_msg_id, int status);
    bool UpdateImagePath(const QString &image_id, const QString &local_path);
    QVector<ChatMessage> GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    QVector<ChatMessage> SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    bool DeleteMessages(int uid1, int uid2);
    bool DeleteMessageByTimestamp(qint64 ts);  // Phase 6 — 单条删除

    DbService(const DbService &) = delete;
    DbService &operator=(const DbService &) = delete;

private:
    DbService();
    ~DbService();

    bool CreateTables(QSqlDatabase &db);
    bool EnsureColumn(QSqlDatabase &db, const QString &table, const QString &column, const QString &definition);
    qint64 FindMessageId(QSqlDatabase &db, const ChatMessage &msg);
    QSqlDatabase &GetOrCreateThreadConnection();
    void CloseAllThreadConnections();

    QSqlDatabase _main_thread_db;
    QString _main_thread_connection_name;
    QString _db_path;
    bool _initialized;
    QMutex _init_mutex;
};

#endif

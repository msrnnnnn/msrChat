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
    int bubbleWidth = 0;
    int bubbleHeight = 0;
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
    QVector<ChatMessage> GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    QVector<ChatMessage> SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    bool DeleteMessages(int uid1, int uid2);

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

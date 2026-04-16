#ifndef DBMGR_H
#define DBMGR_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QMap>
#include <QThreadStorage>

struct ChatMessage {
    qint64 id;
    int from_uid;
    int to_uid;
    QString content;
    qint64 timestamp;
    int status;
};

class DbMgr
{
public:
    static DbMgr& Instance();
    
    bool Init(const QString& db_path);
    static void Destroy();
    
    bool SaveMessage(const ChatMessage& msg);
    QVector<ChatMessage> GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    QVector<ChatMessage> SearchMessages(int uid1, int uid2, const QString& keyword, int limit = 50);
    bool DeleteMessages(int uid1, int uid2);
    
    DbMgr(const DbMgr&) = delete;
    DbMgr& operator=(const DbMgr&) = delete;

private:
    DbMgr();
    ~DbMgr();
    
    bool CreateTables(QSqlDatabase& db);
    QSqlDatabase& GetOrCreateThreadConnection();
    void CloseAllThreadConnections();
    
    QSqlDatabase _main_thread_db;
    QString _main_thread_connection_name;
    QString _db_path;
    bool _initialized;
    QMutex _init_mutex;
};

#endif

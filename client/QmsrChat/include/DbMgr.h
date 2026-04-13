#ifndef DBMGR_H
#define DBMGR_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QMutexLocker>

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
    void Shutdown();
    
    bool SaveMessage(const ChatMessage& msg);
    QVector<ChatMessage> GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    QVector<ChatMessage> SearchMessages(int uid1, int uid2, const QString& keyword, int limit = 50);
    bool DeleteMessages(int uid1, int uid2);
    
    DbMgr(const DbMgr&) = delete;
    DbMgr& operator=(const DbMgr&) = delete;

private:
    DbMgr() = default;
    ~DbMgr() = default;
    
    bool CreateTables();
    
    QSqlDatabase _db;
    QMutex _mutex;
    QString _connection_name;
};

#endif

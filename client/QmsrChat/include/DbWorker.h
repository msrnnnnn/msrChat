#ifndef DBWORKER_H
#define DBWORKER_H

#include "DbMgr.h"
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QVector>
#include <atomic>

class DbWorker : public QObject
{
    Q_OBJECT

public:
    explicit DbWorker(QObject *parent = nullptr);
    ~DbWorker();

public slots:
    void slot_init(const QString &db_path);
    void slot_save_message(const ChatMessage &msg);
    void slot_get_messages(int uid1, int uid2, qint64 before_time, int limit);
    void slot_search_messages(int uid1, int uid2, const QString &keyword, int limit);
    void slot_delete_messages(int uid1, int uid2);
    void slot_stop();

signals:
    void sig_messages_loaded(const QVector<ChatMessage> &messages);
    void sig_messages_saved(bool success);
    void sig_messages_deleted(bool success);
    void sig_error(const QString &error);

private:
    QMutex _mutex;
    bool _dbInitialized;
    std::atomic<bool> _stop_flag;
};

class DbThreadPool : public QObject
{
    Q_OBJECT

public:
    static DbThreadPool &Instance();
    static void Destroy();

    bool Init(const QString &db_path);
    void Shutdown();

    void SaveMessage(const ChatMessage &msg);
    void GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    void SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    void DeleteMessages(int uid1, int uid2);

signals:
    void sig_messages_loaded(const QVector<ChatMessage> &messages);
    void sig_messages_saved(bool success);
    void sig_messages_deleted(bool success);
    void sig_error(const QString &error);

private:
    DbThreadPool();
    ~DbThreadPool();
    DbThreadPool(const DbThreadPool &) = delete;
    DbThreadPool &operator=(const DbThreadPool &) = delete;

    void cleanup();

    QThread *_thread;
    DbWorker *_worker;

    static DbThreadPool *_instance;
};

#endif

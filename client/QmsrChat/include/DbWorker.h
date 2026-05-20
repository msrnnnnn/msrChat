#ifndef DBWORKER_H
#define DBWORKER_H

#include "DbService.h"
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

    bool isDbInitialized() const { return _dbInitialized; }

public slots:
    void slot_init(const QString &db_path);
    void slot_save_message(const ChatMessage &msg);
    void slot_update_message_status(const QString &client_msg_id, int status);
    void slot_get_messages(int uid1, int uid2, qint64 before_time, int limit);
    void slot_search_messages(int uid1, int uid2, const QString &keyword, int limit);
    void slot_delete_messages(int uid1, int uid2);
    void slot_stop();
    void slot_db_destroy();
    void stopAsync();

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

class DbThreadManager : public QObject
{
    Q_OBJECT

public:
    static DbThreadManager &Instance();

    bool Init(const QString &db_path);
    void Shutdown();
    void cleanup();

    void SaveMessage(const ChatMessage &msg);
    void UpdateMessageStatus(const QString &client_msg_id, int status);
    void GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    void SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    void DeleteMessages(int uid1, int uid2);

signals:
    void sig_messages_loaded(const QVector<ChatMessage> &messages);
    void sig_messages_saved(bool success);
    void sig_messages_deleted(bool success);
    void sig_error(const QString &error);

private:
    DbThreadManager();
    ~DbThreadManager();
    DbThreadManager(const DbThreadManager &) = delete;
    DbThreadManager &operator=(const DbThreadManager &) = delete;

    QThread *_thread;
    DbWorker *_worker;
};

#endif

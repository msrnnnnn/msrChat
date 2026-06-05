#ifndef DBWORKER_H
#define DBWORKER_H

#include "DbService.h"
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
    void slot_update_image_path(const QString &image_id, const QString &local_path);
    void slot_get_messages(int uid1, int uid2, qint64 before_time, int limit);
    void slot_search_messages(int uid1, int uid2, const QString &keyword, int limit);
    void slot_delete_messages(int uid1, int uid2);
    void slot_delete_message_by_timestamp(qint64 ts);  // Phase 6
    void slot_db_destroy();
    void stopAsync();

signals:
    void sig_messages_loaded(const QVector<ChatMessage> &messages);
    void sig_messages_saved(bool success);
    void sig_messages_deleted(bool success);
    void sig_error(const QString &error);

private:
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
    void UpdateImagePath(const QString &image_id, const QString &local_path);
    void GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    void SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    void DeleteMessages(int uid1, int uid2);
    void DeleteMessageByTimestamp(qint64 ts);  // Phase 6

signals:
    void sig_messages_loaded(const QVector<ChatMessage> &messages);
    void sig_messages_saved(bool success);
    void sig_messages_deleted(bool success);
    void sig_error(const QString &error);

    void sig_init_db(const QString &db_path);
    void sig_destroy_db();
    void sig_save_msg(const ChatMessage &msg);
    void sig_update_msg_status(const QString &client_msg_id, int status);
    void sig_update_image_path(const QString &image_id, const QString &local_path);
    void sig_get_msgs(int uid1, int uid2, qint64 before_time, int limit);
    void sig_search_msgs(int uid1, int uid2, const QString &keyword, int limit);
    void sig_delete_msgs(int uid1, int uid2);
    void sig_delete_msg_by_ts(qint64 ts);  // Phase 6

private:
    DbThreadManager();
    ~DbThreadManager();
    DbThreadManager(const DbThreadManager &) = delete;
    DbThreadManager &operator=(const DbThreadManager &) = delete;

    QThread *_thread;
    DbWorker *_worker;
};

#endif

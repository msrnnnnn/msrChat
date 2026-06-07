/**
 * @file DbWorker.h
 * @brief 数据库异步工作线程
 * @details DbWorker 运行在独立 QThread 中，通过信号槽接收数据库操作请求；DbThreadManager 管理线程生命周期，对外提供同步式 API。
 */
#ifndef DBWORKER_H
#define DBWORKER_H

#include "DbService.h"
#include <QObject>
#include <QThread>
#include <QVector>
#include <atomic>

/**
 * @brief 数据库异步操作工作对象
 * @details 运行在独立 QThread 事件循环中，通过信号槽接收数据库请求并执行，结果通过信号返回。支持通过 stop_flag 优雅停止。
 */
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
    void slot_mark_message_recalled(qint64 ts, int current_uid);  // 撤回持久化
    void slot_db_destroy();
    /**
     * @brief 请求停止异步工作循环
     */
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

/**
 * @brief 数据库线程管理器单例
 * @details 创建并管理专属数据库工作线程，对外提供同步式 API（内部通过信号槽跨线程调度），所有数据库操作排队在单一线程中执行。
 */
class DbThreadManager : public QObject
{
    Q_OBJECT

public:
    static DbThreadManager &Instance();

    /**
     * @brief 启动数据库工作线程并初始化
     */
    bool Init(const QString &db_path);
    /**
     * @brief 停止工作线程并等待退出
     */
    void Shutdown();
    /**
     * @brief 清理资源（在 QCoreApplication::aboutToQuit 时调用）
     */
    void cleanup();

    void SaveMessage(const ChatMessage &msg);
    void UpdateMessageStatus(const QString &client_msg_id, int status);
    void UpdateImagePath(const QString &image_id, const QString &local_path);
    void GetMessages(int uid1, int uid2, qint64 before_time = LLONG_MAX, int limit = 50);
    void SearchMessages(int uid1, int uid2, const QString &keyword, int limit = 50);
    void DeleteMessages(int uid1, int uid2);
    void DeleteMessageByTimestamp(qint64 ts);  // Phase 6
    void MarkMessageRecalled(qint64 ts, int current_uid);  // 撤回持久化

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
    void sig_mark_msg_recalled(qint64 ts, int current_uid);  // 撤回持久化

private:
    DbThreadManager();
    ~DbThreadManager();
    DbThreadManager(const DbThreadManager &) = delete;
    DbThreadManager &operator=(const DbThreadManager &) = delete;

    QThread *_thread;
    DbWorker *_worker;
};

#endif

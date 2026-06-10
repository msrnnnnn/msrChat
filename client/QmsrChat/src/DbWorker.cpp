/**
 * @file    DbWorker.cpp
 * @brief   数据库工作线程实现（DbWorker + DbThreadManager 跨线程信号槽桥接）
 */
#include "DbWorker.h"
#include <QDebug>

/**
 * @brief DbWorker 构造函数
 * @param parent 父 QObject
 */
DbWorker::DbWorker(QObject *parent)
    : QObject(parent),
      _dbInitialized(false),
      _stop_flag(false)
{
}

/**
 * @brief 析构函数
 */
DbWorker::~DbWorker()
{
}

/**
 * @brief 异步设置停止标志
 * @details 使用 std::atomic 保证跨线程可见性，DB 操作槽函数在检测到 _stop_flag 后拒绝新请求
 */
void DbWorker::stopAsync()
{
    _stop_flag.store(true);
}

/**
 * @brief 销毁数据库（工作线程内调用）
 * @details 仅在 _dbInitialized 时执行，通过 DbService::Destroy() 关闭所有线程连接
 */
void DbWorker::slot_db_destroy()
{

    if (_dbInitialized)
    {
        DbService::Destroy();
        _dbInitialized = false;
        qDebug() << "DbWorker database destroyed in thread" << QThread::currentThreadId();
    }
}

/**
 * @brief 初始化数据库（工作线程内调用）
 * @param db_path 数据库文件路径
 * @details 通过 Qt::BlockingQueuedConnection 从主线程同步调用，保证 Init 完成后再继续
 */
void DbWorker::slot_init(const QString &db_path)
{

    _dbInitialized = DbService::Instance().Init(db_path);

    if (_dbInitialized)
    {
        qDebug() << "DbWorker initialized in thread" << QThread::currentThreadId();
    }
    else
    {
        qCritical() << "DbWorker failed to initialize database in thread" << QThread::currentThreadId();
    }
}

/**
 * @brief 保存消息到数据库（工作线程内调用）
 * @param msg 消息结构体
 * @details 先检查 _stop_flag 和 _dbInitialized，通过 sigMessagesSaved 返回结果
 */
void DbWorker::slot_save_message(const ChatMessage &msg)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring save message request";
        emit sigMessagesSaved(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError(QString("Database not initialized"));
        emit sigMessagesSaved(false);
        return;
    }

    bool success = DbService::Instance().SaveMessage(msg);
    emit sigMessagesSaved(success);

    if (!success)
    {
        emit sigError(QString("Failed to save message: %1").arg(msg.id));
    }
}

/**
 * @brief 更新消息状态（工作线程内调用）
 * @param client_msg_id 客户端消息 ID
 * @param status 新状态
 */
void DbWorker::slot_update_message_status(const QString &client_msg_id, int status)
{

    if (_stop_flag.load())
    {
        emit sigMessagesSaved(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError(QString("Database not initialized"));
        emit sigMessagesSaved(false);
        return;
    }

    const bool success = DbService::Instance().UpdateMessageStatus(client_msg_id, status);
    emit sigMessagesSaved(success);

    if (!success)
    {
        emit sigError(QString("Failed to update message status: %1").arg(client_msg_id));
    }
}

/**
 * @brief 更新图片路径（工作线程内调用，Phase D）
 * @param image_id 图片 UUID
 * @param local_path 本地文件路径
 */
void DbWorker::slot_update_image_path(const QString &image_id, const QString &local_path)
{
    if (_stop_flag.load() || !_dbInitialized)
    {
        return;
    }

    DbService::Instance().UpdateImagePath(image_id, local_path);
}

/**
 * @brief 加载聊天历史消息（工作线程内调用）
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @param before_time 时间戳上限
 * @param limit 每页条数
 */
void DbWorker::slot_get_messages(int uid1, int uid2, qint64 before_time, int limit)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring get messages request";
        emit sigMessagesLoaded(QVector<ChatMessage>());
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError(QString("Database not initialized"));
        emit sigMessagesLoaded(QVector<ChatMessage>());
        return;
    }

    QVector<ChatMessage> messages = DbService::Instance().GetMessages(uid1, uid2, before_time, limit);
    emit sigMessagesLoaded(messages);
}

/**
 * @brief 搜索消息（工作线程内调用）
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @param keyword 搜索关键词
 * @param limit 返回条数限制
 */
void DbWorker::slot_search_messages(int uid1, int uid2, const QString &keyword, int limit)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring search messages request";
        emit sigMessagesLoaded(QVector<ChatMessage>());
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError(QString("Database not initialized"));
        emit sigMessagesLoaded(QVector<ChatMessage>());
        return;
    }

    QVector<ChatMessage> messages = DbService::Instance().GetMessages(uid1, uid2, LLONG_MAX, limit, keyword);
    emit sigMessagesLoaded(messages);
}

/**
 * @brief 删除两个用户之间的所有聊天记录（工作线程内调用）
 * @param uid1 用户 A
 * @param uid2 用户 B
 */
void DbWorker::slot_delete_messages(int uid1, int uid2)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring delete messages request";
        emit sigMessagesDeleted(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError(QString("Database not initialized"));
        emit sigMessagesDeleted(false);
        return;
    }

    bool success = DbService::Instance().DeleteMessages(uid1, uid2);
    emit sigMessagesDeleted(success);

    if (!success)
    {
        emit sigError(QString("Failed to delete messages between %1 and %2").arg(uid1).arg(uid2));
    }
}

// === Phase 6 — 单条删除 ===

/**
 * @brief 按时间戳删除单条消息（工作线程内调用，Phase 6）
 * @param ts 消息时间戳
 */
void DbWorker::slot_delete_message_by_timestamp(qint64 ts)
{
    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring delete message by timestamp request";
        emit sigMessagesDeleted(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError(QString("Database not initialized"));
        emit sigMessagesDeleted(false);
        return;
    }

    bool success = DbService::Instance().DeleteMessageByTimestamp(ts);
    emit sigMessagesDeleted(success);

    if (!success)
    {
        emit sigError(QString("Failed to delete message by timestamp: %1").arg(ts));
    }
}

/**
 * @brief 标记消息为已撤回（工作线程内调用）
 * @param ts 消息时间戳
 * @param current_uid 当前用户 ID
 */
void DbWorker::slot_mark_message_recalled(qint64 ts, int current_uid)
{
    if (_stop_flag.load())
    {
        emit sigError("DbWorker is stopping, ignoring mark recalled");
        return;
    }

    if (!_dbInitialized)
    {
        emit sigError("Database not initialized");
        return;
    }

    bool success = DbService::Instance().MarkMessageRecalled(ts, current_uid);
    if (!success)
    {
        emit sigError(QString("Failed to mark message recalled: ts=%1").arg(ts));
    }
    // 成功 / 失败都通过 sigMessagesSaved 通道发（不阻塞调用方）
    emit sigMessagesSaved(success);
}

/**
 * @brief 获取 DbThreadManager 单例
 * @return 单例引用
 */
DbThreadManager &DbThreadManager::Instance()
{
    static DbThreadManager instance;
    return instance;
}

/**
 * @brief DbThreadManager 构造函数
 * @details 注册 ChatMessage 和 QVector<ChatMessage> 到 Qt 元对象系统，
 *          以支持跨线程信号槽传递自定义类型
 */
DbThreadManager::DbThreadManager()
    : QObject(nullptr),
      _thread(nullptr),
      _worker(nullptr)
{
    qRegisterMetaType<ChatMessage>("ChatMessage");
    qRegisterMetaType<QVector<ChatMessage>>("QVector<ChatMessage>");
}

DbThreadManager::~DbThreadManager()
{
    cleanup();
}

/**
 * @brief 清理工作线程
 * @details 先 stopAsync 通知拒绝新请求 → 发射 sigDestroyDb 清理 DB → quit + wait(5000ms) 等待线程结束。
 *          超时后放弃等待防止死锁，由 OS 在线程退出时回收资源（安全：SQLite 连接由 QThreadStorage 管理）
 */
void DbThreadManager::cleanup()
{
    if (_thread != nullptr)
    {
        if (_worker != nullptr)
        {
            _worker->stopAsync();
            emit sigDestroyDb();
        }

        _thread->quit();

        if (!_thread->wait(5000))
        {
            qCritical() << "CRITICAL: DbThreadManager thread did not finish in 5000ms. "
                           "Abandoning manual cleanup to prevent SQLite corruption. "
                           "Worker thread will be released by OS on process exit.";
            _worker = nullptr;
        }
        else
        {
            delete _worker;
            _worker = nullptr;
        }

        delete _thread;
        _thread = nullptr;

        qDebug() << "DbThreadManager thread stopped safely";
    }

    qDebug() << "DbThreadManager cleanup completed";
}

/**
 * @brief 初始化工作线程和信号槽桥接
 * @param db_path 数据库文件路径
 * @return 初始化是否成功
 * @details 创建 QThread + DbWorker，moveToThread 将 Worker 移入工作线程，
 *          连接所有跨线程信号槽（BlockingQueuedConnection 用于 init/destroy，QueuedConnection 用于日常操作）。
 *          Worker 信号通过 DbThreadManager 转发到主线程
 */
bool DbThreadManager::Init(const QString &db_path)
{
    if (_thread != nullptr)
    {
        qWarning() << "DbThreadManager already initialized";
        return true;
    }

    _thread = new QThread(this);
    _worker = new DbWorker();
    _worker->moveToThread(_thread);

    // 删除 finished->deleteLater：会导致 deleteLater 投递到死线程的事件队列

    connect(_worker, &DbWorker::sigMessagesLoaded, this, &DbThreadManager::sigMessagesLoaded, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sigMessagesSaved, this, &DbThreadManager::sigMessagesSaved, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sigMessagesDeleted, this, &DbThreadManager::sigMessagesDeleted, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sigError, this, &DbThreadManager::sigError, Qt::QueuedConnection);

    connect(this, &DbThreadManager::sigInitDb, _worker, &DbWorker::slot_init, Qt::BlockingQueuedConnection);
    connect(this, &DbThreadManager::sigDestroyDb, _worker, &DbWorker::slot_db_destroy, Qt::BlockingQueuedConnection);
    connect(this, &DbThreadManager::sigSaveMsg, _worker, &DbWorker::slot_save_message, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigUpdateMsgStatus, _worker, &DbWorker::slot_update_message_status, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigUpdateImagePath, _worker, &DbWorker::slot_update_image_path, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigGetMsgs, _worker, &DbWorker::slot_get_messages, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigSearchMsgs, _worker, &DbWorker::slot_search_messages, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigDeleteMsgs, _worker, &DbWorker::slot_delete_messages, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigDeleteMsgByTs, _worker, &DbWorker::slot_delete_message_by_timestamp, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sigMarkMsgRecalled, _worker, &DbWorker::slot_mark_message_recalled, Qt::QueuedConnection);

    _thread->start();

    emit sigInitDb(db_path);

    if (!_worker->isDbInitialized())
    {
        qCritical() << "Failed to initialize DbService on worker thread";
        cleanup();
        return false;
    }

    qDebug() << "DbThreadManager initialized successfully";
    return true;
}

/**
 * @brief 桥接：主线程 → 工作线程 保存消息
 * @param msg 消息结构体
 */
void DbThreadManager::SaveMessage(const ChatMessage &msg)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigSaveMsg(msg);
}

/**
 * @brief 桥接：主线程 → 工作线程 更新消息状态
 * @param client_msg_id 客户端消息 ID
 * @param status 新状态
 */
void DbThreadManager::UpdateMessageStatus(const QString &client_msg_id, int status)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigUpdateMsgStatus(client_msg_id, status);
}

/**
 * @brief 桥接：主线程 → 工作线程 更新图片路径
 * @param image_id 图片 UUID
 * @param local_path 本地路径
 */
void DbThreadManager::UpdateImagePath(const QString &image_id, const QString &local_path)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigUpdateImagePath(image_id, local_path);
}

/**
 * @brief 桥接：主线程 → 工作线程 加载历史消息
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @param before_time 时间戳上限
 * @param limit 每页条数
 */
void DbThreadManager::GetMessages(int uid1, int uid2, qint64 before_time, int limit)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigGetMsgs(uid1, uid2, before_time, limit);
}

/**
 * @brief 桥接：主线程 → 工作线程 搜索消息
 * @param uid1 用户 A
 * @param uid2 用户 B
 * @param keyword 关键词
 * @param limit 返回条数
 */
void DbThreadManager::SearchMessages(int uid1, int uid2, const QString &keyword, int limit)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigSearchMsgs(uid1, uid2, keyword, limit);
}

/**
 * @brief 桥接：主线程 → 工作线程 删除会话历史
 * @param uid1 用户 A
 * @param uid2 用户 B
 */
void DbThreadManager::DeleteMessages(int uid1, int uid2)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigDeleteMsgs(uid1, uid2);
}

/**
 * @brief 桥接：主线程 → 工作线程 单条删除
 * @param ts 消息时间戳
 */
void DbThreadManager::DeleteMessageByTimestamp(qint64 ts)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigDeleteMsgByTs(ts);
}

/**
 * @brief 桥接：主线程 → 工作线程 标记撤回
 * @param ts 消息时间戳
 * @param current_uid 当前用户 ID
 */
void DbThreadManager::MarkMessageRecalled(qint64 ts, int current_uid)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sigMarkMsgRecalled(ts, current_uid);
}

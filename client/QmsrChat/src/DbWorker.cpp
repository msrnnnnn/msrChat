#include "DbWorker.h"
#include <QDebug>

DbWorker::DbWorker(QObject *parent)
    : QObject(parent),
      _dbInitialized(false),
      _stop_flag(false)
{
}

DbWorker::~DbWorker()
{
}

void DbWorker::stopAsync()
{
    _stop_flag.store(true);
}

void DbWorker::slot_db_destroy()
{

    if (_dbInitialized)
    {
        DbService::Destroy();
        _dbInitialized = false;
        qDebug() << "DbWorker database destroyed in thread" << QThread::currentThreadId();
    }
}

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

void DbWorker::slot_save_message(const ChatMessage &msg)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring save message request";
        emit sig_messages_saved(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sig_error(QString("Database not initialized"));
        emit sig_messages_saved(false);
        return;
    }

    bool success = DbService::Instance().SaveMessage(msg);
    emit sig_messages_saved(success);

    if (!success)
    {
        emit sig_error(QString("Failed to save message: %1").arg(msg.id));
    }
}

void DbWorker::slot_update_message_status(const QString &client_msg_id, int status)
{

    if (_stop_flag.load())
    {
        emit sig_messages_saved(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sig_error(QString("Database not initialized"));
        emit sig_messages_saved(false);
        return;
    }

    const bool success = DbService::Instance().UpdateMessageStatus(client_msg_id, status);
    emit sig_messages_saved(success);

    if (!success)
    {
        emit sig_error(QString("Failed to update message status: %1").arg(client_msg_id));
    }
}

void DbWorker::slot_update_image_path(const QString &image_id, const QString &local_path)
{
    if (_stop_flag.load() || !_dbInitialized)
    {
        return;
    }

    DbService::Instance().UpdateImagePath(image_id, local_path);
}

void DbWorker::slot_get_messages(int uid1, int uid2, qint64 before_time, int limit)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring get messages request";
        emit sig_messages_loaded(QVector<ChatMessage>());
        return;
    }

    if (!_dbInitialized)
    {
        emit sig_error(QString("Database not initialized"));
        emit sig_messages_loaded(QVector<ChatMessage>());
        return;
    }

    QVector<ChatMessage> messages = DbService::Instance().GetMessages(uid1, uid2, before_time, limit);
    emit sig_messages_loaded(messages);
}

void DbWorker::slot_search_messages(int uid1, int uid2, const QString &keyword, int limit)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring search messages request";
        emit sig_messages_loaded(QVector<ChatMessage>());
        return;
    }

    if (!_dbInitialized)
    {
        emit sig_error(QString("Database not initialized"));
        emit sig_messages_loaded(QVector<ChatMessage>());
        return;
    }

    QVector<ChatMessage> messages = DbService::Instance().SearchMessages(uid1, uid2, keyword, limit);
    emit sig_messages_loaded(messages);
}

void DbWorker::slot_delete_messages(int uid1, int uid2)
{

    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring delete messages request";
        emit sig_messages_deleted(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sig_error(QString("Database not initialized"));
        emit sig_messages_deleted(false);
        return;
    }

    bool success = DbService::Instance().DeleteMessages(uid1, uid2);
    emit sig_messages_deleted(success);

    if (!success)
    {
        emit sig_error(QString("Failed to delete messages between %1 and %2").arg(uid1).arg(uid2));
    }
}

// === Phase 6 — 单条删除 ===

void DbWorker::slot_delete_message_by_timestamp(qint64 ts)
{
    if (_stop_flag.load())
    {
        qDebug() << "DbWorker is stopping, ignoring delete message by timestamp request";
        emit sig_messages_deleted(false);
        return;
    }

    if (!_dbInitialized)
    {
        emit sig_error(QString("Database not initialized"));
        emit sig_messages_deleted(false);
        return;
    }

    bool success = DbService::Instance().DeleteMessageByTimestamp(ts);
    emit sig_messages_deleted(success);

    if (!success)
    {
        emit sig_error(QString("Failed to delete message by timestamp: %1").arg(ts));
    }
}

DbThreadManager &DbThreadManager::Instance()
{
    static DbThreadManager instance;
    return instance;
}

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

void DbThreadManager::cleanup()
{
    if (_thread != nullptr)
    {
        if (_worker != nullptr)
        {
            _worker->stopAsync();
            emit sig_destroy_db();
        }

        _thread->quit();

        if (!_thread->wait(5000))
        {
            qCritical() << "CRITICAL: DbThreadManager thread did not finish in 5000ms. "
                           "Abandoning manual cleanup to prevent SQLite corruption. "
                           "Worker thread will be released by OS on process exit.";
            _worker = nullptr;
        }

        delete _thread;
        _thread = nullptr;

        qDebug() << "DbThreadManager thread stopped safely";
    }

    qDebug() << "DbThreadManager cleanup completed";
}

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

    connect(_worker, &DbWorker::sig_messages_loaded, this, &DbThreadManager::sig_messages_loaded, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sig_messages_saved, this, &DbThreadManager::sig_messages_saved, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sig_messages_deleted, this, &DbThreadManager::sig_messages_deleted, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sig_error, this, &DbThreadManager::sig_error, Qt::QueuedConnection);

    connect(this, &DbThreadManager::sig_init_db, _worker, &DbWorker::slot_init, Qt::BlockingQueuedConnection);
    connect(this, &DbThreadManager::sig_destroy_db, _worker, &DbWorker::slot_db_destroy, Qt::BlockingQueuedConnection);
    connect(this, &DbThreadManager::sig_save_msg, _worker, &DbWorker::slot_save_message, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sig_update_msg_status, _worker, &DbWorker::slot_update_message_status, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sig_update_image_path, _worker, &DbWorker::slot_update_image_path, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sig_get_msgs, _worker, &DbWorker::slot_get_messages, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sig_search_msgs, _worker, &DbWorker::slot_search_messages, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sig_delete_msgs, _worker, &DbWorker::slot_delete_messages, Qt::QueuedConnection);
    connect(this, &DbThreadManager::sig_delete_msg_by_ts, _worker, &DbWorker::slot_delete_message_by_timestamp, Qt::QueuedConnection);

    _thread->start();

    emit sig_init_db(db_path);

    if (!_worker->isDbInitialized())
    {
        qCritical() << "Failed to initialize DbService on worker thread";
        cleanup();
        return false;
    }

    qDebug() << "DbThreadManager initialized successfully";
    return true;
}

void DbThreadManager::SaveMessage(const ChatMessage &msg)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_save_msg(msg);
}

void DbThreadManager::UpdateMessageStatus(const QString &client_msg_id, int status)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_update_msg_status(client_msg_id, status);
}

void DbThreadManager::UpdateImagePath(const QString &image_id, const QString &local_path)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_update_image_path(image_id, local_path);
}

void DbThreadManager::GetMessages(int uid1, int uid2, qint64 before_time, int limit)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_get_msgs(uid1, uid2, before_time, limit);
}

void DbThreadManager::SearchMessages(int uid1, int uid2, const QString &keyword, int limit)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_search_msgs(uid1, uid2, keyword, limit);
}

void DbThreadManager::DeleteMessages(int uid1, int uid2)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_delete_msgs(uid1, uid2);
}

void DbThreadManager::DeleteMessageByTimestamp(qint64 ts)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadManager not initialized";
        return;
    }

    emit sig_delete_msg_by_ts(ts);
}

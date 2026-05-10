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

void DbWorker::slot_stop()
{
    _stop_flag.store(true);
    qDebug() << "DbWorker stop flag set in thread" << QThread::currentThreadId();
}

void DbWorker::stopAsync()
{
    _stop_flag.store(true);
}

void DbWorker::slot_init(const QString &db_path)
{
    QMutexLocker locker(&_mutex);

    Q_UNUSED(db_path);
    _dbInitialized = true;

    qDebug() << "DbWorker initialized in thread" << QThread::currentThreadId();
}

void DbWorker::slot_save_message(const ChatMessage &msg)
{
    QMutexLocker locker(&_mutex);

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

    bool success = DbMgr::Instance().SaveMessage(msg);
    emit sig_messages_saved(success);

    if (!success)
    {
        emit sig_error(QString("Failed to save message: %1").arg(msg.id));
    }
}

void DbWorker::slot_update_message_status(const QString &client_msg_id, int status)
{
    QMutexLocker locker(&_mutex);

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

    const bool success = DbMgr::Instance().UpdateMessageStatus(client_msg_id, status);
    emit sig_messages_saved(success);

    if (!success)
    {
        emit sig_error(QString("Failed to update message status: %1").arg(client_msg_id));
    }
}

void DbWorker::slot_get_messages(int uid1, int uid2, qint64 before_time, int limit)
{
    QMutexLocker locker(&_mutex);

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

    QVector<ChatMessage> messages = DbMgr::Instance().GetMessages(uid1, uid2, before_time, limit);
    emit sig_messages_loaded(messages);
}

void DbWorker::slot_search_messages(int uid1, int uid2, const QString &keyword, int limit)
{
    QMutexLocker locker(&_mutex);

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

    QVector<ChatMessage> messages = DbMgr::Instance().SearchMessages(uid1, uid2, keyword, limit);
    emit sig_messages_loaded(messages);
}

void DbWorker::slot_delete_messages(int uid1, int uid2)
{
    QMutexLocker locker(&_mutex);

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

    bool success = DbMgr::Instance().DeleteMessages(uid1, uid2);
    emit sig_messages_deleted(success);

    if (!success)
    {
        emit sig_error(QString("Failed to delete messages between %1 and %2").arg(uid1).arg(uid2));
    }
}

DbThreadPool &DbThreadPool::Instance()
{
    static DbThreadPool instance;
    return instance;
}

DbThreadPool::DbThreadPool()
    : QObject(nullptr),
      _thread(nullptr),
      _worker(nullptr)
{
    qRegisterMetaType<ChatMessage>("ChatMessage");
    qRegisterMetaType<QVector<ChatMessage>>("QVector<ChatMessage>");
}

DbThreadPool::~DbThreadPool()
{
    cleanup();
}

void DbThreadPool::cleanup()
{
    if (_thread != nullptr)
    {
        if (_worker != nullptr)
        {
            _worker->stopAsync();
        }

        _thread->quit();

        if (!_thread->wait(5000))
        {
            qCritical() << "CRITICAL: DbThreadPool thread did not finish in 5000ms. "
                           "Abandoning manual cleanup to prevent SQLite corruption. "
                           "Worker thread will be released by OS on process exit.";
            _worker = nullptr;
        }

        delete _thread;
        _thread = nullptr;

        qDebug() << "DbThreadPool thread stopped safely";
    }

    DbMgr::Destroy();
    qDebug() << "DbThreadPool cleanup completed (timeout path)";
}

bool DbThreadPool::Init(const QString &db_path)
{
    if (_thread != nullptr)
    {
        qWarning() << "DbThreadPool already initialized";
        return true;
    }

    if (!DbMgr::Instance().Init(db_path))
    {
        qWarning() << "Failed to initialize DbMgr";
        return false;
    }

    _thread = new QThread(this);
    _worker = new DbWorker();
    _worker->moveToThread(_thread);

    // 删除 finished->deleteLater：会导致 deleteLater 投递到死线程的事件队列

    connect(_worker, &DbWorker::sig_messages_loaded, this, &DbThreadPool::sig_messages_loaded, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sig_messages_saved, this, &DbThreadPool::sig_messages_saved, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sig_messages_deleted, this, &DbThreadPool::sig_messages_deleted, Qt::QueuedConnection);
    connect(_worker, &DbWorker::sig_error, this, &DbThreadPool::sig_error, Qt::QueuedConnection);

    _thread->start();

    QMetaObject::invokeMethod(_worker, "slot_init", Qt::QueuedConnection, Q_ARG(QString, db_path));

    qDebug() << "DbThreadPool initialized successfully";
    return true;
}

void DbThreadPool::Shutdown()
{
    cleanup();
}

void DbThreadPool::SaveMessage(const ChatMessage &msg)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadPool not initialized";
        return;
    }

    QMetaObject::invokeMethod(_worker, "slot_save_message", Qt::QueuedConnection, Q_ARG(ChatMessage, msg));
}

void DbThreadPool::UpdateMessageStatus(const QString &client_msg_id, int status)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadPool not initialized";
        return;
    }

    QMetaObject::invokeMethod(
        _worker, "slot_update_message_status", Qt::QueuedConnection, Q_ARG(QString, client_msg_id), Q_ARG(int, status));
}

void DbThreadPool::GetMessages(int uid1, int uid2, qint64 before_time, int limit)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadPool not initialized";
        return;
    }

    QMetaObject::invokeMethod(
        _worker, "slot_get_messages", Qt::QueuedConnection, Q_ARG(int, uid1), Q_ARG(int, uid2),
        Q_ARG(qint64, before_time), Q_ARG(int, limit));
}

void DbThreadPool::SearchMessages(int uid1, int uid2, const QString &keyword, int limit)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadPool not initialized";
        return;
    }

    QMetaObject::invokeMethod(
        _worker, "slot_search_messages", Qt::QueuedConnection, Q_ARG(int, uid1), Q_ARG(int, uid2),
        Q_ARG(QString, keyword), Q_ARG(int, limit));
}

void DbThreadPool::DeleteMessages(int uid1, int uid2)
{
    if (_worker == nullptr)
    {
        qWarning() << "DbThreadPool not initialized";
        return;
    }

    QMetaObject::invokeMethod(
        _worker, "slot_delete_messages", Qt::QueuedConnection, Q_ARG(int, uid1), Q_ARG(int, uid2));
}

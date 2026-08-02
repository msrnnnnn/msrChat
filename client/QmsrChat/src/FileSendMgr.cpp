/**
 * @file    FileSendMgr.cpp
 * @brief   文件发送管理器实现
 * @details 管理文件分片发送任务，支持断点续传。使用 QMutex 保护 _tasks 映射表，
 *          每次发送 64KB 分片，通过 protobuf 序列化后经 TcpMgr 长连接发送。
 */
#include "FileSendMgr.h"
#include "TcpMgr.h"
#include "Message.pb.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>
#include <QThreadPool>

namespace
{
class SendMd5Runnable : public QRunnable
{
public:
    SendMd5Runnable(int64_t task_id, const QString &filepath) : _task_id(task_id), _filepath(filepath)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QFile file(_filepath);
        if (!file.open(QIODevice::ReadOnly))
        {
            QMetaObject::invokeMethod(&FileSendMgr::Instance(), "OnMd5Computed", Qt::QueuedConnection,
                                      Q_ARG(int64_t, _task_id), Q_ARG(QString, QString()));
            return;
        }
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(&file);
        QString md5 = QString::fromLatin1(hash.result().toHex());
        QMetaObject::invokeMethod(&FileSendMgr::Instance(), "OnMd5Computed", Qt::QueuedConnection,
                                  Q_ARG(int64_t, _task_id), Q_ARG(QString, md5));
    }

private:
    int64_t _task_id;
    QString _filepath;
};
}

/**
 * @brief 获取单例实例（Meyers' Singleton）
 */
FileSendMgr &FileSendMgr::Instance()
{
    static FileSendMgr instance;
    return instance;
}

/**
 * @brief 构造与析构（单例，无特殊初始化/清理逻辑）
 */
FileSendMgr::FileSendMgr()
{
}
FileSendMgr::~FileSendMgr()
{
}

/**
 * @brief 启动文件发送任务
 * @param task_id 任务 ID（毫秒级时间戳）
 * @param to_uid 目标用户 ID
 * @param filepath 文件路径
 * @details 以只读模式打开文件，初始化发送状态并注册任务
 */
void FileSendMgr::StartSend(int64_t task_id, int to_uid, const QString &filepath)
{
    qDebug() << "[FileSendMgr] StartSend called, task_id:" << task_id << "to_uid:" << to_uid << "filepath:" << filepath;
    // RAII 加锁，保护 _tasks 映射表
    QMutexLocker locker(&_mutex);
    // 防止重复启动同一任务
    if (_tasks.find(task_id) != _tasks.end())
    {
        qDebug() << "Send task already exists:" << task_id;
        return;
    }

    FileSendTask newTask;
    newTask.task_id = task_id;
    newTask.to_uid = to_uid;
    newTask.filepath = filepath;
    // unique_ptr 管理 QFile 生命周期，RAII 确保文件句柄最终被释放
    newTask.file = std::make_unique<QFile>(filepath);
    if (!newTask.file->open(QIODevice::ReadOnly))
    {
        emit sigSendComplete(task_id, false, "Failed to open file");
        return;
    }
    newTask.total_size = newTask.file->size();
    newTask.sent_size = 0;
    newTask.active = true;

    qDebug() << "Start send task:" << task_id << "file:" << filepath << "size:" << newTask.total_size;
    _tasks.insert_or_assign(task_id, std::move(newTask));

    QThreadPool::globalInstance()->start(new SendMd5Runnable(task_id, filepath));
}

/**
 * @brief 接收方就绪回调（FileRsp 触发）
 * @param task_id 任务 ID
 * @param offset 已接收偏移量（断点续传时 > 0）
 * @details 首次发送或断点续传：seek 到 offset，然后填满发送窗口
 */
void FileSendMgr::OnRecvReady(int64_t task_id, int64_t offset)
{
    qDebug() << "[FileSendMgr] OnRecvReady called, task_id:" << task_id << "offset:" << offset;
    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end() || !it->second.active)
        return;

    FileSendTask &task = it->second;

    // 断点续传：接收方已有部分数据，seek 到对应位置
    if (offset > task.sent_size && offset < task.total_size)
    {
        if (!task.file->seek(offset))
        {
            emit sigSendComplete(task_id, false, "Failed to seek file");
            _tasks.erase(it);
            return;
        }
        task.sent_size = offset;
    }

    // 填满发送窗口（首次调用 in_flight=0，直接发满）
    while (task.active && task.in_flight < task.window_size && task.sent_size < task.total_size)
    {
        SendNextChunk(task);
    }

    if (!task.active)
        _tasks.erase(it);
}

/**
 * @brief 分片确认回调（FileAck 触发）
 * @param task_id 任务 ID
 * @param received 接收端已确认的累计偏移量
 * @details 滑动窗口流控：释放一个 in_flight 槽位，继续发送；传输完成时通知 UI
 */
void FileSendMgr::OnChunkAck(int64_t task_id, int64_t received)
{
    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end() || !it->second.active)
        return;

    FileSendTask &task = it->second;

    // 接收端确认全部数据已收到 → 传输完成
    if (received >= task.total_size)
    {
        task.file->close();
        emit sigSendComplete(task_id, true, {});
        _tasks.erase(it);
        return;
    }

    // 释放一个 in_flight 槽位，继续发送
    if (task.in_flight > 0)
        task.in_flight--;

    while (task.active && task.in_flight < task.window_size && task.sent_size < task.total_size)
    {
        SendNextChunk(task);
    }

    if (!task.active)
        _tasks.erase(it);
}

/**
 * @brief 取消文件发送任务
 * @param task_id 任务 ID
 * @param reason 取消原因
 */
void FileSendMgr::CancelSend(int64_t task_id, const QString &reason)
{
    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it != _tasks.end())
    {
        it->second.file->close();
        emit sigSendComplete(task_id, false, reason.isEmpty() ? QStringLiteral("cancelled") : reason);
        _tasks.erase(it);
    }
}

void FileSendMgr::OnMd5Computed(int64_t task_id, const QString &md5)
{
    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
        return;

    FileSendTask &task = it->second;
    task.md5 = md5;
    task.md5_ready = true;
    emit sigMd5Ready(task_id, md5);
}

/**
 * @brief 发送下一个文件分片
 * @param task 文件发送任务
 * @details 每次发送 64KB 分片，发送完成后发射 sigSendProgress
 */
void FileSendMgr::SendNextChunk(FileSendTask &task)
{
    if (!task.active || !task.file->isOpen())
    {
        return;
    }

    QByteArray data = task.file->read(kChunkSize);
    if (data.isEmpty())
    {
        if (task.sent_size >= task.total_size)
        {
            emit sigSendComplete(task.task_id, true, "");
        }
        else
        {
            emit sigSendComplete(task.task_id, false, "Read empty chunk before EOF");
        }
        task.active = false;
        task.file->close();
        return;
    }

    qmsrchat::FileChunk chunk;
    chunk.set_task_id(task.task_id);
    chunk.set_offset(task.sent_size);
    chunk.set_size(data.size());
    chunk.set_data(data.toStdString());

    std::string serialized;
    if (!chunk.SerializeToString(&serialized))
    {
        emit sigSendComplete(task.task_id, false, "Failed to serialize FileChunk");
        task.active = false;
        task.file->close();
        return;
    }

    // 通过 TcpMgr 长连接发送 protobuf 序列化后的分片数据
    TcpMgr::Instance()->slotSendData(RequestType::MSG_FILE_CHUNK,
                                     QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    task.sent_size += data.size();
    task.in_flight++;

    // 发射进度信号，供 UI 层更新进度条
    int progress = static_cast<int>((task.sent_size * 100) / task.total_size);
    emit sigSendProgress(task.task_id, progress, task.sent_size, task.total_size);
}
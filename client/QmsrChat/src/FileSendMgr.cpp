#include "FileSendMgr.h"
#include "TcpMgr.h"
#include "Message.pb.h"
#include <QDebug>
#include <QFileInfo>

FileSendMgr &FileSendMgr::Instance()
{
    static FileSendMgr instance;
    return instance;
}

FileSendMgr::FileSendMgr() {}
FileSendMgr::~FileSendMgr() {}

void FileSendMgr::StartSend(int64_t task_id, int to_uid, const QString &filepath)
{
    QMutexLocker locker(&_mutex);
    if (_tasks.find(task_id) != _tasks.end())
    {
        qDebug() << "Send task already exists:" << task_id;
        return;
    }

    FileSendTask newTask;
    newTask.task_id = task_id;
    newTask.to_uid = to_uid;
    newTask.filepath = filepath;
    newTask.file = std::make_unique<QFile>(filepath);
    if (!newTask.file->open(QIODevice::ReadOnly))
    {
        emit sigSendComplete(task_id, false, "Failed to open file");
        return;
    }
    newTask.total_size = newTask.file->size();
    newTask.sent_size = 0;
    newTask.active = true;
    _tasks.insert_or_assign(task_id, std::move(newTask));

    qDebug() << "Start send task:" << task_id << "file:" << filepath << "size:" << newTask.total_size;
}

void FileSendMgr::OnRecvReady(int64_t task_id, int64_t offset)
{
    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end() || !it->second.active)
    {
        return;
    }

    FileSendTask &task = it->second;
    if (offset > 0 && offset < task.total_size)
    {
        if (!task.file->seek(offset))
        {
            emit sigSendComplete(task_id, false, "Failed to seek file");
            _tasks.erase(it);
            return;
        }
        task.sent_size = offset;
    }

    SendNextChunk(task);

    if (!task.active)
    {
        _tasks.erase(it);
    }
}

void FileSendMgr::CancelSend(int64_t task_id)
{
    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it != _tasks.end())
    {
        it->second.file->close();
        _tasks.erase(it);
    }
}

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

    TcpMgr::Instance()->slot_send_data(
        RequestType::MSG_FILE_CHUNK, QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    task.sent_size += data.size();

    int progress = static_cast<int>((task.sent_size * 100) / task.total_size);
    emit sigSendProgress(task.task_id, progress, task.sent_size, task.total_size);
}
#include "FileRecvMgr.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

FileWriteTask::FileWriteTask(int64_t task_id, QByteArray data, const QString &temp_filepath)
    : _task_id(task_id),
      _data(std::move(data)),
      _temp_filepath(temp_filepath)
{
    setAutoDelete(true);
}

void FileWriteTask::run()
{
    QFile file(_temp_filepath);
    bool success = false;
    QString error;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        error = "Failed to open file for writing";
        qDebug() << error << _temp_filepath;
    }
    else
    {
        qint64 written = file.write(_data);
        if (written == _data.size())
        {
            success = true;
        }
        else
        {
            error = QString("Write failed: wrote %1 of %2 bytes").arg(written).arg(_data.size());
            qDebug() << error;
        }
        file.close();
    }

    emit sigWriteComplete(_task_id, success, error);
}

FileRecvMgr &FileRecvMgr::Instance()
{
    static FileRecvMgr instance;
    return instance;
}

FileRecvMgr::FileRecvMgr()
{
    _threadPool.setMaxThreadCount(4);
    _threadPool.setExpiryTimeout(30000);
}

FileRecvMgr::~FileRecvMgr()
{
    _threadPool.waitForDone(5000);
    QMutexLocker locker(&_mutex);
    for (auto &pair : _tasks)
    {
        if (!pair.second.temp_filepath.empty() && QFile::exists(pair.second.temp_filepath_qstring))
        {
            QFile::remove(pair.second.temp_filepath_qstring);
        }
    }
}

QString FileRecvMgr::GetTempDir() const
{
    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/msrchat_files";
    QDir dir(temp_dir);
    if (!dir.exists())
    {
        dir.mkpath(temp_dir);
    }
    return temp_dir;
}

QString FileRecvMgr::GetFinalPath(const std::string &filename) const
{
    QString download_dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/msrchat";
    QDir dir(download_dir);
    if (!dir.exists())
    {
        dir.mkpath(download_dir);
    }
    return download_dir + "/" + QString::fromStdString(filename);
}

bool FileRecvMgr::OpenTempFile(FileRecvTask &task)
{
    QString temp_dir = GetTempDir();
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString temp_filename =
        QString::number(task.task_id) + "_" + timestamp + "_" + QString::fromStdString(task.filename) + ".tmp";
    task.temp_filepath_qstring = temp_dir + "/" + temp_filename;
    task.temp_filepath = task.temp_filepath_qstring.toStdString();

    QFile file(task.temp_filepath_qstring);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "Failed to create temp file:" << task.temp_filepath_qstring;
        return false;
    }
    file.close();
    return true;
}

void FileRecvMgr::StartRecv(
    int64_t task_id, int from_uid, const std::string &filename, int64_t total_size, const std::string &md5)
{
    QMutexLocker locker(&_mutex);

    if (_tasks.find(task_id) != _tasks.end())
    {
        qDebug() << "Task already exists:" << task_id;
        return;
    }

    FileRecvTask task;
    task.task_id = task_id;
    task.from_uid = from_uid;
    task.filename = filename;
    task.total_size = total_size;
    task.received_size = 0;
    task.md5 = md5;
    task.completed = false;

    if (!OpenTempFile(task))
    {
        emit SigRecvComplete(task_id, "", false, "Failed to create temp file");
        return;
    }

    _tasks[task_id] = task;
    qDebug() << "Start recv task:" << task_id << "file:" << QString::fromStdString(filename) << "size:" << total_size;
}

void FileRecvMgr::WriteChunk(int64_t task_id, const char *data, size_t len)
{
    FileRecvTask *task = nullptr;

    {
        QMutexLocker locker(&_mutex);
        auto it = _tasks.find(task_id);
        if (it == _tasks.end() || it->second.completed)
        {
            qDebug() << "Task not found or already completed:" << task_id;
            return;
        }
        task = &it->second;
        task->received_size += len;
    }

    QByteArray data_copy(data, static_cast<int>(len));
    FileWriteTask *write_task = new FileWriteTask(task_id, std::move(data_copy), task->temp_filepath_qstring);

    QObject::connect(
        write_task, &FileWriteTask::sigWriteComplete, this, &FileRecvMgr::onWriteComplete, Qt::QueuedConnection);

    _threadPool.start(write_task);

    UpdateProgress(task_id);
}

void FileRecvMgr::onWriteComplete(int64_t task_id, bool success, const QString &error)
{
    if (!success)
    {
        qDebug() << "Async write failed for task:" << task_id << error;
        QMutexLocker locker(&_mutex);
        auto it = _tasks.find(task_id);
        if (it != _tasks.end())
        {
            emit SigRecvComplete(task_id, "", false, error);
            _tasks.erase(it);
        }
        return;
    }

    QMutexLocker locker(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return;
    }

    FileRecvTask &task = it->second;
    if (task.received_size >= task.total_size)
    {
        CompleteTask(task_id);
    }
}

void FileRecvMgr::UpdateProgress(int64_t task_id)
{
    FileRecvTask *task = nullptr;
    {
        QMutexLocker locker(&_mutex);
        auto it = _tasks.find(task_id);
        if (it == _tasks.end())
        {
            return;
        }
        task = &it->second;
    }

    int progress = static_cast<int>((task->received_size * 100) / task->total_size);
    emit SigRecvProgress(task_id, progress, task->received_size, task->total_size);
}

void FileRecvMgr::OnChunkAck(int64_t task_id, int64_t received_size)
{
    QMutexLocker locker(&_mutex);

    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return;
    }

    qDebug() << "Chunk ACK:" << task_id << "offset:" << received_size;
}

void FileRecvMgr::CompleteTask(int64_t task_id)
{
    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return;
    }

    FileRecvTask &task = it->second;
    QString final_path = GetFinalPath(task.filename);

    if (QFile::exists(final_path))
    {
        QFile::remove(final_path);
    }

    if (QFile::rename(task.temp_filepath_qstring, final_path))
    {
        bool md5_ok = true;
        if (!task.md5.empty())
        {
            md5_ok = ValidateMd5(final_path, task.md5);
        }

        if (md5_ok)
        {
            task.completed = true;
            emit SigRecvComplete(task_id, final_path, true, "");
            qDebug() << "File recv completed:" << task_id << final_path;
        }
        else
        {
            QFile::remove(final_path);
            emit SigRecvComplete(task_id, "", false, "MD5 validation failed");
            qDebug() << "File MD5 validation failed:" << task_id;
        }
    }
    else
    {
        emit SigRecvComplete(task_id, "", false, "Failed to rename file");
        qDebug() << "Failed to rename file:" << task_id;
    }

    _tasks.erase(it);
}

void FileRecvMgr::CancelRecv(int64_t task_id)
{
    QMutexLocker locker(&_mutex);

    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return;
    }

    FileRecvTask &task = it->second;

    if (!task.temp_filepath.empty() && QFile::exists(task.temp_filepath_qstring))
    {
        QFile::remove(task.temp_filepath_qstring);
    }

    _tasks.erase(it);
    qDebug() << "Task cancelled:" << task_id;
}

bool FileRecvMgr::ValidateMd5(const QString &filepath, const std::string &expected_md5)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    const int chunk_size = 4096;
    char buffer[chunk_size];

    while (!file.atEnd())
    {
        qint64 bytes_read = file.read(buffer, chunk_size);
        if (bytes_read <= 0)
        {
            break;
        }
        hash.addData(buffer, bytes_read);
    }

    QString actual_md5 = QString::fromLatin1(hash.result().toHex());
    return actual_md5.toStdString() == expected_md5;
}

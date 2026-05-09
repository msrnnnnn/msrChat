#include "FileRecvMgr.h"
#include <QDir>
#include <QMutexLocker>

FileRecvMgr::FileRecvMgr()
    : QObject(nullptr)
{
}

FileRecvMgr::~FileRecvMgr()
{
}

FileRecvMgr &FileRecvMgr::Instance()
{
    static FileRecvMgr instance;
    return instance;
}

bool FileRecvMgr::StartRecv(
    int64_t task_id, int from_uid, const std::string &filename, int64_t total_size, const std::string &md5,
    QString *error)
{
    QMutexLocker lock(&_mutex);

    if (_tasks.contains(task_id))
    {
        return Fail(error, "task already exists");
    }
    if (total_size <= 0)
    {
        return Fail(error, "invalid total size");
    }

    FileRecvTask task;
    task.task_id = task_id;
    task.from_uid = from_uid;
    task.filename = QString::fromStdString(filename);
    task.total_size = total_size;
    task.md5 = QString::fromStdString(md5);
    task.temp_filepath = BuildTempPath(task_id, task.filename);
    task.final_filepath = BuildFinalPath(task.filename);
    task.file = std::make_unique<QFile>(task.temp_filepath);

    if (!task.file->open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return Fail(error, "open temp file failed");
    }

    _tasks.insert(task_id, std::move(task));
    return true;
}

bool FileRecvMgr::WriteChunk(
    int64_t task_id, int64_t offset, const QByteArray &data, int64_t *committed, QString *error)
{
    QMutexLocker lock(&_mutex);

    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return Fail(error, "task not found");
    }

    FileRecvTask &task = it.value();

    if (offset != task.received_size)
    {
        return Fail(error, "unexpected chunk offset");
    }
    if (data.isEmpty())
    {
        return Fail(error, "empty chunk");
    }
    if (task.received_size + data.size() > task.total_size)
    {
        return Fail(error, "chunk exceeds total size");
    }

    if (!task.file->seek(offset))
    {
        return Fail(error, "seek failed");
    }

    const qint64 written = task.file->write(data);
    if (written != data.size())
    {
        return Fail(error, "write failed");
    }
    if (!task.file->flush())
    {
        return Fail(error, "flush failed");
    }

    task.received_size += written;
    if (committed)
    {
        *committed = task.received_size;
    }

    emit SigRecvProgress(
        task_id, CalcProgress(task.received_size, task.total_size), task.received_size, task.total_size);

    if (task.received_size == task.total_size)
    {
        return CompleteTask(it, error);
    }
    return true;
}

void FileRecvMgr::CancelRecv(int64_t task_id)
{
    QMutexLocker lock(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return;
    }

    it->file->close();
    QFile::remove(it->temp_filepath);
    _tasks.erase(it);
}

bool FileRecvMgr::CompleteTask(QHash<int64_t, FileRecvTask>::iterator it, QString *error)
{
    FileRecvTask &task = it.value();
    task.file->close();

    QFile::remove(task.final_filepath);
    if (!QFile::rename(task.temp_filepath, task.final_filepath))
    {
        return FailAndEmit(task.task_id, error, "rename failed");
    }

    if (!task.md5.isEmpty() && CalcMd5(task.final_filepath) != task.md5)
    {
        QFile::remove(task.final_filepath);
        return FailAndEmit(task.task_id, error, "md5 mismatch");
    }

    const QString finalPath = task.final_filepath;
    const int64_t taskId = task.task_id;
    _tasks.erase(it);
    emit SigRecvComplete(taskId, finalPath, true, {});
    return true;
}

bool FileRecvMgr::FailAndEmit(int64_t task_id, QString *error, const char *message)
{
    if (error)
    {
        *error = QString::fromLatin1(message);
    }
    emit SigRecvComplete(task_id, {}, false, QString::fromLatin1(message));
    return false;
}

bool FileRecvMgr::Fail(QString *error, const char *message) const
{
    if (error)
    {
        *error = QString::fromLatin1(message);
    }
    return false;
}

int FileRecvMgr::CalcProgress(int64_t received, int64_t total) const
{
    return total == 0 ? 0 : static_cast<int>((received * 100) / total);
}

QString FileRecvMgr::GetTempDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/msrchat";
    QDir().mkpath(base);
    return base;
}

QString FileRecvMgr::GetFinalPath(const QString &filename) const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/msrchat";
    QDir().mkpath(base);
    return base + "/" + filename;
}

QString FileRecvMgr::BuildTempPath(int64_t task_id, const QString &fileName) const
{
    return GetTempDir() + "/" + QString::number(task_id) + "_" + fileName + ".part";
}

QString FileRecvMgr::BuildFinalPath(const QString &fileName) const
{
    return GetFinalPath(fileName);
}

QString FileRecvMgr::CalcMd5(const QString &filepath) const
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

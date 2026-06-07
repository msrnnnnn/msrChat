/**
 * @file    FileRecvMgr.cpp
 * @brief   文件接收管理器实现（分片接收、MD5 校验、临时文件管理）
 */
#include "FileRecvMgr.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThreadPool>
#include <QDebug>
#include <QUuid>

/**
 * @brief 匿名命名空间：内部辅助函数与 QRunnable
 */
namespace
{
/**
 * @brief 同步计算文件 MD5 值
 * @param filepath 文件路径
 * @return MD5 十六进制字符串（失败返回空）
 */
QString CalcMd5Sync(const QString &filepath)
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

/**
 * @brief 异步 MD5 计算任务（QRunnable）
 * @details 在 QThreadPool 中执行 MD5 计算，完成后通过 QMetaObject::invokeMethod
 *          跨线程回调 FileRecvMgr::OnMd5Computed
 */
class Md5Runnable : public QRunnable
{
public:
    Md5Runnable(int64_t task_id, const QString &filepath)
        : _task_id(task_id), _filepath(filepath)
    {
    }

    void run() override
    {
        QString md5 = CalcMd5Sync(_filepath);
        bool ok = !md5.isEmpty();
        QMetaObject::invokeMethod(
            &FileRecvMgr::Instance(), "OnMd5Computed", Qt::QueuedConnection,
            Q_ARG(int64_t, _task_id), Q_ARG(QString, _filepath), Q_ARG(bool, ok), Q_ARG(QString, md5));
    }

private:
    int64_t _task_id;
    QString _filepath;
};
}  // namespace

/**
 * @brief 构造函数
 */
FileRecvMgr::FileRecvMgr()
    : QObject(nullptr)
{
}

/**
 * @brief 析构函数，清理所有接收任务
 * @details 关闭文件、删除临时 .part 文件、释放内存
 */
FileRecvMgr::~FileRecvMgr()
{
    QMutexLocker lock(&_mutex);
    for (auto it = _tasks.begin(); it != _tasks.end(); ++it)
    {
        if (it.value())
        {
            it.value()->file.close();
            QFile::remove(it.value()->temp_filepath);
            delete it.value();
        }
    }
    _tasks.clear();
}

/**
 * @brief 获取单例实例
 * @return FileRecvMgr 引用
 */
FileRecvMgr &FileRecvMgr::Instance()
{
    static FileRecvMgr instance;
    return instance;
}

/**
 * @brief 启动文件接收任务
 * @param task_id 任务 ID
 * @param from_uid 发送方用户 ID
 * @param filename 文件名
 * @param total_size 文件总大小
 * @param md5 MD5 校验值（可选）
 * @param error 错误信息输出
 * @return 启动是否成功
 * @details 在临时目录创建 .part 文件，等待数据写入
 */
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

    auto *task = new FileRecvTask();
    task->task_id = task_id;
    task->from_uid = from_uid;
    task->filename = QString::fromStdString(filename);
    task->total_size = total_size;
    task->md5 = QString::fromStdString(md5);
    task->temp_filepath = BuildTempPath(task_id, task->filename);
    task->final_filepath = BuildFinalPath(task->filename);
    task->file.setFileName(task->temp_filepath);

    QFileInfo info(task->temp_filepath);
    if (info.exists() && info.size() > 0 && info.size() < total_size)
    {
        task->received_size = info.size();
        if (!task->file.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            delete task;
            return Fail(error, "open partial temp file failed");
        }
    }
    else
    {
        if (!task->file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            delete task;
            return Fail(error, "open temp file failed");
        }
    }

    _tasks.insert(task_id, task);
    return true;
}

/**
 * @brief 写入文件分片数据
 * @param task_id 任务 ID
 * @param offset 数据偏移量
 * @param data 分片数据
 * @param committed 已提交偏移量输出
 * @param error 错误信息输出
 * @return 写入是否成功
 * @details 必须按序写入，写入完成后验证 MD5（如果提供）
 */
bool FileRecvMgr::WriteChunk(
    int64_t task_id, int64_t offset, const QByteArray &data, int64_t *committed, QString *error)
{
    QMutexLocker lock(&_mutex);

    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return Fail(error, "task not found");
    }

    FileRecvTask *task = it.value();
    if (!task)
    {
        return Fail(error, "task not found");
    }

    if (offset != task->received_size)
    {
        return Fail(error, "unexpected chunk offset");
    }
    if (data.isEmpty())
    {
        return Fail(error, "empty chunk");
    }
    if (task->received_size + data.size() > task->total_size)
    {
        return Fail(error, "chunk exceeds total size");
    }

    if (!task->file.seek(offset))
    {
        return Fail(error, "seek failed");
    }

    const qint64 written = task->file.write(data);
    if (written != data.size())
    {
        return Fail(error, "write failed");
    }
    if (!task->file.flush())
    {
        return Fail(error, "flush failed");
    }

    task->received_size += written;
    if (committed)
    {
        *committed = task->received_size;
    }

    emit SigRecvProgress(
        task_id, CalcProgress(task->received_size, task->total_size), task->received_size, task->total_size);

    if (task->received_size == task->total_size)
    {
        return CompleteTask(it, error);
    }
    return true;
}

/**
 * @brief 取消文件接收任务
 * @param task_id 任务 ID
 * @details 关闭文件、删除临时文件、释放内存
 */
void FileRecvMgr::CancelRecv(int64_t task_id)
{
    QMutexLocker lock(&_mutex);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end())
    {
        return;
    }

    FileRecvTask *task = it.value();
    if (task)
    {
        task->file.close();
        QFile::remove(task->temp_filepath);
        delete task;
    }
    _tasks.erase(it);
}

/**
 * @brief 获取指定 task 的已接收大小
 * @param task_id 任务 ID
 * @return 已接收字节数（任务不存在返回 0）
 */
int64_t FileRecvMgr::GetReceivedSize(int64_t task_id) const
{
    QMutexLocker lock(&_mutex);
    auto it = _tasks.find(task_id);
    return (it != _tasks.end()) ? it.value()->received_size : 0;
}

/**
 * @brief 完成接收任务
 * @param it 任务迭代器
 * @param error 错误信息输出
 * @return 是否成功
 * @details 关闭文件、重命名临时文件为正式文件名、异步计算 MD5 校验
 */
bool FileRecvMgr::CompleteTask(QHash<int64_t, FileRecvTask *>::iterator it, QString *error)
{
    FileRecvTask *task = it.value();
    if (!task)
    {
        return Fail(error, "task not found");
    }

    task->file.close();

    QFile::remove(task->final_filepath);
    if (!QFile::rename(task->temp_filepath, task->final_filepath))
    {
        const bool emitted = FailAndEmit(task->task_id, error, "rename failed");
        QFile::remove(task->temp_filepath);
        delete task;
        _tasks.erase(it);
        return emitted;
    }

    if (task->md5.isEmpty())
    {
        const QString finalPath = task->final_filepath;
        const int64_t taskId = task->task_id;
        delete task;
        _tasks.erase(it);
        emit SigRecvComplete(taskId, finalPath, true, {});
        return true;
    }

    _pendingMd5.insert(task->task_id, task->md5);
    QThreadPool::globalInstance()->start(new Md5Runnable(task->task_id, task->final_filepath));
    delete task;
    _tasks.erase(it);
    return true;
}

/**
 * @brief 失败处理：写错误信息并发射 SigRecvComplete(false)
 * @param task_id 任务 ID
 * @param error 错误信息输出参数
 * @param message 错误消息
 * @return false
 */
bool FileRecvMgr::FailAndEmit(int64_t task_id, QString *error, const char *message)
{
    if (error)
    {
        *error = QString::fromLatin1(message);
    }
    emit SigRecvComplete(task_id, {}, false, QString::fromLatin1(message));
    return false;
}

/**
 * @brief 失败处理（仅写错误信息，不发射信号）
 * @param error 错误信息输出参数
 * @param message 错误消息
 * @return false
 */
bool FileRecvMgr::Fail(QString *error, const char *message) const
{
    if (error)
    {
        *error = QString::fromLatin1(message);
    }
    return false;
}

/**
 * @brief 计算接收进度百分比
 * @param received 已接收字节
 * @param total 总字节
 * @return 0-100 百分比
 */
int FileRecvMgr::CalcProgress(int64_t received, int64_t total) const
{
    return total == 0 ? 0 : static_cast<int>((received * 100) / total);
}

/**
 * @brief 获取临时文件目录
 * @return 临时目录路径（<Temp>/msrchat）
 */
QString FileRecvMgr::GetTempDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/msrchat";
    QDir().mkpath(base);
    return base;
}

/**
 * @brief 根据文件名确定最终保存路径
 * @param filename 文件名
 * @return 最终路径
 * @details 如果文件名主干是合法 UUID → 判定为图片，落入 client_image_cache/ 目录；
 *          否则落入 DownloadLocation/msrchat/ 目录
 */
QString FileRecvMgr::GetFinalPath(const QString &filename) const
{
    // Image mode detection: filename stem is a UUID → 落到 client_image_cache/
    // (server uses image_id as task_id and encodes format in filename "{uuid}.{ext}")
    int dot = filename.lastIndexOf('.');
    if (dot > 0)
    {
        QString stem = filename.left(dot);
        QUuid uuid(stem);
        if (!uuid.isNull())
        {
            QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                + "/client_image_cache";
            QDir().mkpath(cache_dir);
            return cache_dir + "/" + filename;
        }
    }
    // 默认路径（普通文件）
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/msrchat";
    QDir().mkpath(base);
    return base + "/" + filename;
}

/**
 * @brief 构建临时文件路径
 * @param task_id 任务 ID
 * @param fileName 文件名
 * @return 临时文件完整路径（含 .part 后缀）
 */
QString FileRecvMgr::BuildTempPath(int64_t task_id, const QString &fileName) const
{
    return GetTempDir() + "/" + QString::number(task_id) + "_" + fileName + ".part";
}

/**
 * @brief 构建最终文件路径
 * @param fileName 文件名
 * @return 最终文件完整路径
 */
QString FileRecvMgr::BuildFinalPath(const QString &fileName) const
{
    return GetFinalPath(fileName);
}

/**
 * @brief MD5 校验结果回调
 * @param task_id 任务 ID
 * @param filepath 文件路径
 * @param success 计算是否成功
 * @param md5 计算出的 MD5 值
 * @details 异步线程池计算的 MD5 与预期值比对，失败则删除文件
 */
void FileRecvMgr::OnMd5Computed(int64_t task_id, const QString &filepath, bool success, const QString &md5)
{
    if (!success)
    {
        QFile::remove(filepath);
        emit SigRecvComplete(task_id, {}, false, "md5 computation failed");
        return;
    }

    QMutexLocker lock(&_mutex);
    auto it = _pendingMd5.find(task_id);
    if (it == _pendingMd5.end())
    {
        QFile::remove(filepath);
        emit SigRecvComplete(task_id, {}, false, "task not found");
        return;
    }

    QString expectedMd5 = it.value();
    _pendingMd5.erase(it);

    if (!expectedMd5.isEmpty() && md5 != expectedMd5)
    {
        QFile::remove(filepath);
        emit SigRecvComplete(task_id, {}, false, "md5 mismatch");
        return;
    }

    emit SigRecvComplete(task_id, filepath, true, {});
}

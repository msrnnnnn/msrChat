#include "ImageDownloadMgr.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

ImageDownloadMgr &ImageDownloadMgr::Instance() {
    static ImageDownloadMgr inst; return inst;
}

ImageDownloadMgr::ImageDownloadMgr() {
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + "/client_image_cache";
    QDir().mkpath(cache_dir);
}

ImageDownloadMgr::~ImageDownloadMgr() = default;

QString ImageDownloadMgr::GetCachePath(const QString &image_id, const QString &ext) const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + "/client_image_cache/" + image_id + "." + ext;
}

bool ImageDownloadMgr::IsCached(const QString &image_id) const
{
    QMutexLocker lock(&_mutex);
    return _cache_index.contains(image_id);
}

void ImageDownloadMgr::Request(const QString &image_id, int retry_count)
{
    QMutexLocker lock(&_mutex);
    if (_pending.contains(image_id) || _cache_index.contains(image_id)) return;
    _pending[image_id] = {retry_count, ""};
    qDebug() << "ImageDownloadMgr::Request" << image_id;
    // 实际：emit sigRequestImageDownload(image_id); 由 ChatController 转发给 TcpMgr
    // 当前 Phase 4 范围：仅 placeholder，待 P4-T2 + TcpMgr 信号集成后填充
}

void ImageDownloadMgr::OnDownloadRsp(int error, const QString &image_id, int64_t /*offset*/)
{
    QMutexLocker lock(&_mutex);
    if (error == 4040)  // ERR_IMAGE_EXPIRED
    {
        qWarning() << "image expired:" << image_id;
        _pending.remove(image_id);
        emit sigImageFailed(image_id, 4040);
        return;
    }
    if (error != 0)
    {
        // 重试
        auto it = _pending.find(image_id);
        if (it != _pending.end() && it->retry_count < kMaxRetries)
        {
            it->retry_count++;
            QTimer::singleShot(kRetryIntervalMs, this, [this, image_id, rc = it->retry_count]() {
                Request(image_id, rc);
            });
        }
        else
        {
            _pending.remove(image_id);
            emit sigImageFailed(image_id, error);
        }
    }
    // error=0: 客户端收到响应后用 FileReq（task_id=image_id）走现有文件通道
}

void ImageDownloadMgr::OnFileRecvComplete(const QString &image_id, const QString &local_path, bool success)
{
    QMutexLocker lock(&_mutex);
    if (!success)
    {
        emit sigImageFailed(image_id, -1);
        _pending.remove(image_id);
        return;
    }
    _cache_index[image_id] = local_path;
    _pending.remove(image_id);
    emit sigImageReady(image_id, local_path);
}
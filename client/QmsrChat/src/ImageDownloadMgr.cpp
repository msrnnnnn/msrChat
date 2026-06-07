/**
 * @file    ImageDownloadMgr.cpp
 * @brief   图片下载管理器实现
 * @details 管理图片下载的缓存、请求派发与重试逻辑。下载通过服务端中转，
 *          客户端收到响应后用 FileReq（task_id=image_id）走现有文件传输通道。
 *          支持本地文件缓存（QStandardPaths::CacheLocation），以及失败重试（最多3次）。
 */
#include "ImageDownloadMgr.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

/**
 * @brief 获取单例实例（Meyers' Singleton）
 */
ImageDownloadMgr &ImageDownloadMgr::Instance() {
    static ImageDownloadMgr inst; return inst;
}

/**
 * @brief 构造函数，确保缓存目录存在
 */
ImageDownloadMgr::ImageDownloadMgr() {
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + "/client_image_cache";
    QDir().mkpath(cache_dir);
}

/**
 * @brief 析构函数（默认实现）
 */
ImageDownloadMgr::~ImageDownloadMgr() = default;

/**
 * @brief 获取图片本地缓存路径
 * @param image_id 图片 ID
 * @param ext 图片扩展名
 * @return 完整的本地缓存文件路径
 */
QString ImageDownloadMgr::GetCachePath(const QString &image_id, const QString &ext) const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + "/client_image_cache/" + image_id + "." + ext;
}

/**
 * @brief 检查图片是否已缓存
 * @param image_id 图片 ID
 * @return 已缓存返回 true
 * @note 线程安全：使用 QMutexLocker 保护 _cache_index
 */
bool ImageDownloadMgr::IsCached(const QString &image_id) const
{
    QMutexLocker lock(&_mutex);
    return _cache_index.contains(image_id);
}

/**
 * @brief 发起图片下载请求
 * @param image_id 图片 ID
 * @param retry_count 当前重试次数（首次调用传 0）
 * @param ext 图片扩展名
 * @details 防重入：若已在 _pending、_cache_index 或 _failed_index 中存在则忽略。
 *          加锁注册后 unlock 再发射信号，避免跨线程信号槽的潜在死锁。
 */
void ImageDownloadMgr::Request(const QString &image_id, int retry_count, const QString &ext)
{
    QMutexLocker lock(&_mutex);
    // 防重入：已请求中 / 已缓存 / 已标记失败 则跳过
    if (_pending.contains(image_id) || _cache_index.contains(image_id) || _failed_index.contains(image_id)) return;
    _pending[image_id] = {retry_count, ext};
    qDebug() << "ImageDownloadMgr::Request" << image_id << "ext:" << ext;
    // 在释放锁后再发射信号，避免接收方同步回调中再次获取锁造成死锁
    lock.unlock();
    emit sigRequestDownload(image_id);
}

/**
 * @brief 处理下载响应
 * @param error 错误码（0 表示成功，4040 表示图片过期）
 * @param image_id 图片 ID
 * @param offset 偏移量（未使用）
 * @details 错误码 4040 表示图片已过期，直接标记失败。
 *          其他错误码进入重试逻辑：最多重试 kMaxRetries 次，间隔 kRetryIntervalMs ms。
 *          成功（error=0）：不做处理，等待后续 FileReq 通道完成实际文件传输。
 */
void ImageDownloadMgr::OnDownloadRsp(int error, const QString &image_id, int64_t /*offset*/)
{
    QMutexLocker lock(&_mutex);
    if (error == 4040)  // ERR_IMAGE_EXPIRED
    {
        qWarning() << "image expired:" << image_id;
        _failed_index.insert(image_id);
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

/**
 * @brief 处理文件接收完成通知
 * @param image_id 图片 ID
 * @param local_path 本地存储路径
 * @param success 是否接收成功
 * @details 成功则将路径写入 _cache_index 并发射 sigImageReady；
 *          失败则从 _pending 移除并发射 sigImageFailed。
 */
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
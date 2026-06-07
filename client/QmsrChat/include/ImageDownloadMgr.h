/**
 * @file    ImageDownloadMgr.h
 * @brief   图片下载管理器
 * @details 管理图片下载请求的队列与缓存：发起下载请求、接收分片数据、本地缓存索引维护及失败重试。
 */
#ifndef IMAGE_DOWNLOAD_MGR_H
#define IMAGE_DOWNLOAD_MGR_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

/**
 * @brief 图片下载管理器（单例）
 * @details 维护下载链路中的三个索引表：等待表、缓存表、失败表。支持自动重试。
 */
class ImageDownloadMgr : public QObject
{
    Q_OBJECT
public:
    static ImageDownloadMgr &Instance();

    /**
     * @brief 请求下载图片
     * @param image_id 图片唯一标识
     * @param retry_count 当前重试次数（内部使用）
     * @param ext 文件扩展名
     */
    void Request(const QString &image_id, int retry_count = 0, const QString &ext = "");
    /**
     * @brief 处理下载响应
     * @param error 错误码（0表示成功）
     * @param image_id 图片ID
     * @param offset 服务器返回的已接收偏移量
     */
    void OnDownloadRsp(int error, const QString &image_id, int64_t offset);
    /**
     * @brief 文件接收完成回调
     * @param image_id 图片ID
     * @param local_path 本地文件路径
     * @param success 是否成功
     */
    void OnFileRecvComplete(const QString &image_id, const QString &local_path, bool success);

    /**
     * @brief 获取缓存的本地路径
     * @param image_id 图片ID
     * @param ext 文件扩展名
     * @return 缓存文件的绝对路径
     */
    QString GetCachePath(const QString &image_id, const QString &ext) const;
    /**
     * @brief 检查图片是否已缓存
     * @param image_id 图片ID
     * @return true 已缓存
     */
    bool IsCached(const QString &image_id) const;

signals:
    /**
     * @brief 图片就绪信号
     * @param image_id 图片ID
     * @param local_path 本地文件路径
     */
    void sigImageReady(const QString &image_id, const QString &local_path);
    /**
     * @brief 图片下载失败信号
     * @param image_id 图片ID
     * @param reason 失败原因码（4040表示已过期）
     */
    void sigImageFailed(const QString &image_id, int reason);
    /**
     * @brief 请求下载授权信号
     * @param image_id 图片ID
     */
    void sigRequestDownload(const QString &image_id);

private:
    ImageDownloadMgr();
    ~ImageDownloadMgr();
    ImageDownloadMgr(const ImageDownloadMgr &) = delete;
    ImageDownloadMgr &operator=(const ImageDownloadMgr &) = delete;

    /**
     * @brief 等待中的下载任务信息
     */
    struct Pending
    {
        int retry_count = 0;  ///< 当前重试次数
        QString ext;          ///< 文件扩展名
    };

    QHash<QString, Pending> _pending;     ///< 等待表：image_id → 下载信息
    QHash<QString, QString> _cache_index; ///< 缓存索引：image_id → 本地绝对路径
    QSet<QString> _failed_index;          ///< 失败索引：已过期/失败的 image_id 集合
    mutable QMutex _mutex;                ///< 线程互斥锁
    static constexpr int kMaxRetries = 3;        ///< 最大重试次数
    static constexpr int kRetryIntervalMs = 5000;///< 重试间隔（毫秒）
};

#endif
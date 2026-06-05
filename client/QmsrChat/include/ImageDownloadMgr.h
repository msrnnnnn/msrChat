#ifndef IMAGE_DOWNLOAD_MGR_H
#define IMAGE_DOWNLOAD_MGR_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class ImageDownloadMgr : public QObject
{
    Q_OBJECT
public:
    static ImageDownloadMgr &Instance();

    void Request(const QString &image_id, int retry_count = 0, const QString &ext = "");
    void OnDownloadRsp(int error, const QString &image_id, int64_t offset);
    void OnFileRecvComplete(const QString &image_id, const QString &local_path, bool success);

    QString GetCachePath(const QString &image_id, const QString &ext) const;
    bool IsCached(const QString &image_id) const;
    bool IsFailed(const QString &image_id) const;

signals:
    void sigImageReady(const QString &image_id, const QString &local_path);
    void sigImageFailed(const QString &image_id, int reason);  // 4040 = expired
    void sigRequestDownload(const QString &image_id);          // Phase D: 请求下载授权

private:
    ImageDownloadMgr();
    ~ImageDownloadMgr();
    ImageDownloadMgr(const ImageDownloadMgr &) = delete;
    ImageDownloadMgr &operator=(const ImageDownloadMgr &) = delete;

    struct Pending
    {
        int retry_count = 0;
        QString ext;
    };

    QHash<QString, Pending> _pending;     // image_id → info
    QHash<QString, QString> _cache_index; // image_id → absolute path
    QSet<QString> _failed_index;          // image_id → expired/failed
    mutable QMutex _mutex;
    static constexpr int kMaxRetries = 3;
    static constexpr int kRetryIntervalMs = 5000;
};

#endif
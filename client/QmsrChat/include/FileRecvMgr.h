#ifndef FILERECVMGR_H
#define FILERECVMGR_H

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>

struct FileRecvTask
{
    int64_t task_id = 0;
    int from_uid = 0;
    QString filename;
    QString temp_filepath;
    QString final_filepath;
    QString md5;
    int64_t total_size = 0;
    int64_t received_size = 0;
    QFile file;
};

class FileRecvMgr : public QObject
{
    Q_OBJECT

public:
    static FileRecvMgr &Instance();

    bool StartRecv(
        int64_t task_id, int from_uid, const std::string &filename, int64_t total_size, const std::string &md5 = "",
        QString *error = nullptr);
    bool WriteChunk(
        int64_t task_id, int64_t offset, const QByteArray &data, int64_t *committed = nullptr,
        QString *error = nullptr);
    void CancelRecv(int64_t task_id);
    int64_t GetReceivedSize(int64_t task_id) const;

signals:
    void SigRecvProgress(int64_t task_id, int progress, int64_t received, int64_t total);
    void SigRecvComplete(int64_t task_id, const QString &filepath, bool success, const QString &error);

public slots:
    void OnMd5Computed(int64_t task_id, const QString &filepath, bool success, const QString &md5);

private:
    FileRecvMgr();
    ~FileRecvMgr();
    FileRecvMgr(const FileRecvMgr &) = delete;
    FileRecvMgr &operator=(const FileRecvMgr &) = delete;

    bool CompleteTask(QHash<int64_t, FileRecvTask *>::iterator it, QString *error);
    bool Fail(QString *error, const char *message) const;
    bool FailAndEmit(int64_t task_id, QString *error, const char *message);
    int CalcProgress(int64_t received, int64_t total) const;
    QString GetTempDir() const;
    QString GetFinalPath(const QString &filename) const;
    QString BuildTempPath(int64_t task_id, const QString &fileName) const;
    QString BuildFinalPath(const QString &fileName) const;

    QHash<int64_t, FileRecvTask *> _tasks;
    QHash<int64_t, QString> _pendingMd5;
    mutable QMutex _mutex;
};

#endif // FILERECVMGR_H

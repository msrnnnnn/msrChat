#ifndef FILERECVMGR_H
#define FILERECVMGR_H

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QRunnable>
#include <QString>
#include <QThreadPool>
#include <map>
#include <string>

struct FileRecvTask
{
    int64_t task_id = 0;
    int from_uid = 0;
    std::string filename;
    int64_t total_size = 0;
    int64_t received_size = 0;
    std::string temp_filepath;
    std::string md5;
    QString temp_filepath_qstring;
    bool completed = false;
};

class FileWriteTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    FileWriteTask(int64_t task_id, QByteArray data, const QString &temp_filepath, QObject *parent = nullptr);
    void run() override;

signals:
    void sigWriteComplete(int64_t task_id, bool success, const QString &error);

private:
    int64_t _task_id;
    QByteArray _data;
    QString _temp_filepath;
};

class FileRecvMgr : public QObject
{
    Q_OBJECT

public:
    static FileRecvMgr &Instance();

    void StartRecv(
        int64_t task_id, int from_uid, const std::string &filename, int64_t total_size, const std::string &md5 = "");
    void WriteChunk(int64_t task_id, const char *data, size_t len);
    void OnChunkAck(int64_t task_id, int64_t received_size);
    void CancelRecv(int64_t task_id);

private slots:
    void onWriteComplete(int64_t task_id, bool success, const QString &error);

signals:
    void SigRecvProgress(int64_t task_id, int progress, int64_t received, int64_t total);
    void SigRecvComplete(int64_t task_id, const QString &filepath, bool success, const QString &error);

private:
    FileRecvMgr();
    ~FileRecvMgr();
    FileRecvMgr(const FileRecvMgr &) = delete;
    FileRecvMgr &operator=(const FileRecvMgr &) = delete;

    bool OpenTempFile(FileRecvTask &task);
    QString GetTempDir() const;
    QString GetFinalPath(const std::string &filename) const;
    bool ValidateMd5(const QString &filepath, const std::string &expected_md5);
    void CompleteTask(int64_t task_id);
    void UpdateProgress(int64_t task_id);

    std::map<int64_t, FileRecvTask> _tasks;
    QMutex _mutex;
    QThreadPool _threadPool;
};

#endif // FILERECVMGR_H

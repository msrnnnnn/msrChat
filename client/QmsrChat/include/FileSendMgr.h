#ifndef FILESENDMGR_H
#define FILESENDMGR_H

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <map>

struct FileSendTask
{
    int64_t task_id = 0;
    int to_uid = 0;
    QString filepath;
    int64_t total_size = 0;
    int64_t sent_size = 0;
    bool active = false;
    QFile file;
};

class FileSendMgr : public QObject
{
    Q_OBJECT
public:
    static FileSendMgr &Instance();

    void StartSend(int64_t task_id, int to_uid, const QString &filepath);
    // 根据接收端返回的 offset 继续发送（支持断点续传）
    void OnRecvReady(int64_t task_id, int64_t offset);
    void CancelSend(int64_t task_id);

signals:
    void sigSendProgress(int64_t task_id, int progress, int64_t sent, int64_t total);
    void sigSendComplete(int64_t task_id, bool success, const QString &error);

private:
    FileSendMgr();
    ~FileSendMgr();
    FileSendMgr(const FileSendMgr &) = delete;
    FileSendMgr &operator=(const FileSendMgr &) = delete;

    void SendNextChunk(FileSendTask &task);

    std::map<int64_t, FileSendTask> _tasks;
    QMutex _mutex;
    static constexpr int kChunkSize = 65536;
};

#endif
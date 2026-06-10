#pragma once
/**
 * @file    FileRecvMgr.h
 * @brief   文件接收管理器
 * @details 管理文件接收任务的生命周期：创建任务、写入分片数据、断点续传校验、MD5完整性校验及任务完成通知。
 */
#ifndef FILERECVMGR_H
#define FILERECVMGR_H

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>

/**
 * @brief 文件接收任务上下文
 */
struct FileRecvTask
{
    int64_t task_id = 0;     ///< 任务ID
    int from_uid = 0;         ///< 发送方用户ID
    QString filename;         ///< 原始文件名
    QString temp_filepath;    ///< 临时文件路径（接收中）
    QString final_filepath;   ///< 最终文件路径（接收完成）
    QString md5;              ///< 文件MD5校验值
    int64_t total_size = 0;   ///< 文件总大小（字节）
    int64_t received_size = 0;///< 已接收大小（字节）
    QFile file;               ///< 文件句柄
};

/**
 * @brief 文件接收管理器（单例）
 * @details 采用分片写入 + MD5异步校验的方式接收文件，支持断点续传。
 */
class FileRecvMgr : public QObject
{
    Q_OBJECT

public:
    static FileRecvMgr &Instance();

    /**
     * @brief 开始接收文件
     * @param task_id 任务ID
     * @param from_uid 发送方用户ID
     * @param filename 文件名
     * @param total_size 文件总大小（字节）
     * @param md5 文件MD5值（可选，用于完整性校验）
     * @param error 错误信息输出参数
     * @return true 任务创建成功
     */
    bool StartRecv(
        int64_t task_id, int from_uid, const std::string &filename, int64_t total_size, const std::string &md5 = "",
        QString *error = nullptr);
    /**
     * @brief 写入文件分片数据
     * @param task_id 任务ID
     * @param offset 数据写入偏移量
     * @param data 分片数据
     * @param committed 输出参数：本次成功写入的字节数
     * @param error 错误信息输出参数
     * @return true 写入成功
     */
    bool WriteChunk(
        int64_t task_id, int64_t offset, const QByteArray &data, int64_t *committed = nullptr,
        QString *error = nullptr);
    /**
     * @brief 取消接收任务
     * @param task_id 任务ID
     */
    void CancelRecv(int64_t task_id);
    /**
     * @brief 获取已接收字节数
     * @param task_id 任务ID
     * @return 已接收的字节数
     */
    int64_t GetReceivedSize(int64_t task_id) const;

signals:
    /**
     * @brief 接收开始信号
     */
    void sigRecvStarted(int64_t task_id, const QString &filename, int64_t total_size);
    /**
     * @brief 接收进度更新信号
     * @param task_id 任务ID
     * @param progress 进度百分比（0-100）
     * @param received 已接收字节数
     * @param total 总字节数
     */
    void sigRecvProgress(int64_t task_id, int progress, int64_t received, int64_t total);
    /**
     * @brief 接收完成信号
     * @param task_id 任务ID
     * @param filepath 最终文件路径
     * @param success 是否成功
     * @param error 错误信息（失败时有效）
     */
    void sigRecvComplete(int64_t task_id, const QString &filepath, bool success, const QString &error);

public slots:
    /**
     * @brief MD5计算完成回调
     * @details 异步MD5计算完成后触发，用于校验文件完整性。
     * @param task_id 任务ID
     * @param filepath 文件路径
     * @param success MD5计算是否成功
     * @param md5 计算得到的MD5值
     */
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

    QHash<int64_t, FileRecvTask *> _tasks;  ///< 活跃任务表：task_id → 任务上下文
    QHash<int64_t, QString> _pendingMd5;     ///< 待MD5校验任务：task_id → 文件路径
    mutable QMutex _mutex;                   ///< 线程互斥锁
};

#endif // FILERECVMGR_H

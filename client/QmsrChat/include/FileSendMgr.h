#pragma once
/**
 * @file    FileSendMgr.h
 * @brief   文件发送管理器
 * @details 管理文件发送任务的生命周期：创建任务、分片发送、断点续传及发送完成通知。
 */
#ifndef FILESENDMGR_H
#define FILESENDMGR_H

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <map>
#include <memory>

/**
 * @brief 文件发送任务上下文
 */
struct FileSendTask
{
    int64_t task_id = 0;
    int to_uid = 0;
    QString filepath;
    int64_t total_size = 0;
    int64_t sent_size = 0;
    bool active = false;
    int in_flight = 0;
    int window_size = 8;
    std::unique_ptr<QFile> file;
};

/**
 * @brief 文件发送管理器（单例）
 * @details 采用固定分片大小分块发送文件，接收端通过 OnRecvReady 反馈偏移量实现断点续传。
 */
class FileSendMgr : public QObject
{
    Q_OBJECT
public:
    static FileSendMgr &Instance();

    /**
     * @brief 开始发送文件
     * @param task_id 任务ID
     * @param to_uid 接收方用户ID
     * @param filepath 本地文件路径
     */
    void StartSend(int64_t task_id, int to_uid, const QString &filepath);
    /**
     * @brief 接收端就绪，从指定偏移量继续发送（支持断点续传）
     * @param task_id 任务ID
     * @param offset 接收端已收到的字节偏移量
     */
    void OnRecvReady(int64_t task_id, int64_t offset);
    /**
     * @brief 取消发送任务
     * @param task_id 任务ID
     */
    void CancelSend(int64_t task_id);

signals:
    /**
     * @brief 发送进度更新信号
     * @param task_id 任务ID
     * @param progress 进度百分比（0-100）
     * @param sent 已发送字节数
     * @param total 总字节数
     */
    void sigSendProgress(int64_t task_id, int progress, int64_t sent, int64_t total);
    /**
     * @brief 发送完成信号
     * @param task_id 任务ID
     * @param success 是否成功
     * @param error 错误信息（失败时有效）
     */
    void sigSendComplete(int64_t task_id, bool success, const QString &error);

private:
    FileSendMgr();
    ~FileSendMgr();
    FileSendMgr(const FileSendMgr &) = delete;
    FileSendMgr &operator=(const FileSendMgr &) = delete;

    /**
     * @brief 发送下一个分片数据块
     * @param task 发送任务引用
     */
    void SendNextChunk(FileSendTask &task);

    std::map<int64_t, FileSendTask> _tasks; ///< 活跃任务表：task_id → 任务上下文
    QMutex _mutex;                           ///< 线程互斥锁
    static constexpr int kChunkSize = 65536; ///< 分片大小（64KB）
};

#endif
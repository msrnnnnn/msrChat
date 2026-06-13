#pragma once
/**
 * @file FileCoordinator.h
 * @brief 文件/图片发送接收协调器
 * @details Phase 5B.6 从 ChatController.cpp 提取。
 *          封装文件传输、图片发送/接收、图片查看器等逻辑。
 */
#ifndef FILECOORDINATOR_H
#define FILECOORDINATOR_H

#include "ChatListModel.h"
#include "ProtocolStructs.h"
#include <QObject>
#include <QString>
#include <QVariantList>

class FileCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit FileCoordinator(QObject *parent = nullptr);

    void setChatModel(ChatListModel *model);
    void setTargetUid(int uid);
    void setCurrentUid(int uid);
    void setMaxReceivedTimestamp(qint64 *ts_ptr);

    Q_INVOKABLE void sendFile(const QString &filePath);
    Q_INVOKABLE void sendImage(const QString &imagePath, const QString &caption);
    Q_INVOKABLE void openImageViewer(const QString &imageId);
    Q_INVOKABLE QVariantList getImageListForViewer() const;

    /**
     * @brief 连接文件/图片相关信号槽
     */
    void connectSignals();
    /**
     * @brief 断开文件/图片相关信号槽
     */
    void disconnectSignals();

    static QString normalizeFilePath(const QString &rawPath);

signals:
    void sigError(const QString &error);
    void sigFileSendStarted(int64_t task_id, QString filename, int64_t total_size);
    void sigFileSendProgress(int64_t task_id, int progress, int64_t sent, int64_t total);
    void sigFileSendComplete(int64_t task_id, bool success, QString error);
    void sigFileRecvProgress(int64_t task_id, int progress, int64_t received, int64_t total);
    void sigFileRecvStarted(int64_t task_id, const QString &filename, int64_t total_size);
    void sigFileRecvComplete(int64_t task_id, const QString &filepath, bool success, const QString &error);
    void sigSendImageMsg(const ChatImageStruct &msg);
    void sigShowImageViewer(QVariantList imageList, int currentIndex);

public slots:
    void slotOnChatImage(const ChatImageStruct &msg);
    void slotOnImageDownloadRsp(const ImageDownloadRspStruct &rsp);

private:
    ChatListModel *_chat_model = nullptr;
    int _target_uid = 0;
    int _current_uid = 0;
    qint64 *_max_received_ts = nullptr;
};

#endif // FILECOORDINATOR_H

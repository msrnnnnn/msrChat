#pragma once
/**
 * @file ChatController.h
 * @brief QML 与 C++ 业务层通信桥梁
 * @details 封装聊天业务逻辑，暴露属性和方法供 QML 调用，监听网络信号并转发给 QML。
 */
#ifndef CHATCONTROLLER_H
#define CHATCONTROLLER_H

#include "ChatListModel.h"
#include "DbWorker.h"
#include "ProtocolStructs.h"
#include "TcpMgr.h"
#include "UserMgr.h"
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QUuid>

struct PendingMessageInfo
{
    qint64 send_time = 0;  ///< 消息发送时间，用于超时检测
};

/**
 * @brief 聊天控制器，封装聊天业务逻辑
 * @details 作为 QML 与 C++ 业务层的通信桥梁，管理消息发送/接收、历史加载、连接状态，通过信号槽与 TcpMgr 和 DbWorker 交互。
 */
class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentUid READ GetCurrentUid NOTIFY sigCurrentUidChanged)
    Q_PROPERTY(int targetUid READ GetTargetUid WRITE setTargetUid NOTIFY sigTargetUidChanged)
    Q_PROPERTY(bool isConnected READ IsConnected NOTIFY sigConnectionStatusChanged)

public:
    explicit ChatController(QObject *parent = nullptr);
    ~ChatController();

    Q_INVOKABLE void sendMessage(const QString &content);
    Q_INVOKABLE void sendFile(const QString &filePath);
    Q_INVOKABLE void sendImage(const QString &imagePath, const QString &caption);
    Q_INVOKABLE void setTargetUid(int uid);
    /**
     * @brief 加载当前会话的历史消息（最近一页）
     */
    Q_INVOKABLE void loadHistory();
    /**
     * @brief 加载更早的历史消息（向上分页）
     */
    Q_INVOKABLE void loadMoreHistory();
    /**
     * @brief 初始化：连接信号槽、启动心跳定时器等
     */
    Q_INVOKABLE void initialize();
    Q_INVOKABLE void openImageViewer(const QString &imageId);
    Q_INVOKABLE QVariantList getImageListForViewer() const;

    // Phase 6 — 右键菜单 6 项 action
    Q_INVOKABLE void actionReply(qint64 timestamp);
    Q_INVOKABLE void actionCopyText(qint64 timestamp);
    Q_INVOKABLE void actionRecall(qint64 timestamp);
    Q_INVOKABLE void actionEdit(qint64 timestamp, const QString &newContent);
    Q_INVOKABLE void actionDelete(qint64 timestamp);

    /**
     * @brief 将离线期间缓冲的消息按序注入消息模型
     */
    void drainBufferedMessages(const QVector<ChatTextMsgStruct> &msgs);

    int GetCurrentUid() const;
    int GetTargetUid() const;
    bool IsConnected() const;
    void setChatModel(ChatListModel *model);

signals:
    void sigCurrentUidChanged();
    void sigTargetUidChanged();
    void sigConnectionStatusChanged();
    void sigError(const QString &error);
    void sigFileSendStarted(int64_t task_id, QString filename, int64_t total_size);
    void sigFileSendProgress(int64_t task_id, int progress, int64_t sent, int64_t total);
    void sigFileSendComplete(int64_t task_id, bool success, QString error);
    void sigFileRecvProgress(int64_t task_id, int progress, int64_t received, int64_t total);
    void sigFileRecvStarted(int64_t task_id, const QString &filename, int64_t total_size);
    void sigFileRecvComplete(int64_t task_id, const QString &filepath, bool success, const QString &error);
    void sigSendImageMsg(const ChatImageStruct &msg);
    void sigSendEditMsg(const ChatEditMsgStruct &msg);
    void sigShowImageViewer(QVariantList imageList, int currentIndex);
    // Phase 6 — 菜单 action signals
    void sigSendRecallMsg(const ChatRecallMsgStruct &msg);
    void sigSetReplyContext(const QString &prefix);

public slots:
    void slotOnChatTextMsg(const ChatTextMsgStruct &msg);
    void slotOnChatAck(const ChatAckStruct &ack);
    void slotOnConnectionStateChanged(bool connected);
    void slotOnOfflineProgress(const OfflineAckStruct &ack);
    void slotOnReconnected();
    void slotOnChatLoginRsp(const ChatLoginRspStruct &rsp);
    /**
     * @brief 将暂存中未应用的撤回消息刷新到当前聊天模型
     */
    void FlushPendingRecalls();
    void slotOnHistoryLoaded(const QVector<ChatMessage> &messages);
    void slotOnMessageSaved(bool success);
    /**
     * @brief 定时清理超时未收到 ACK 的待确认消息
     */
    void slotCleanTimeoutMessages();
    void slotOnChatImage(const ChatImageStruct &msg);
    void slotOnImageDownloadRsp(const ImageDownloadRspStruct &rsp);
    void slotOnChatRecallRsp(const ChatEditAckStruct &ack);
    void slotOnChatEditAck(const ChatEditAckStruct &ack);
    void slotOnChatRecallNotify(const ChatRecallNotifyStruct &n);
    void slotOnChatEditNotify(const ChatEditNotifyStruct &n);

private:
    void ConnectSignals();
    void DisconnectSignals();
    static QString normalizeFilePath(const QString &rawPath);

    int _target_uid;
    int _current_uid;
    bool _is_connected;
    ChatListModel *_chat_model = nullptr;
    QMutex _pending_mutex;
    QHash<QString, PendingMessageInfo> _pending_messages;
    // 撤回 Notify 暂存：slotOnChatRecallNotify 收到时若当前 _chat_model 里没对应 timestamp
    // （比如用户正在和别人聊天），先存这里，切回 A 会话时再 apply
    QHash<qint64, qint64> _pending_recall;  // msg_timestamp → recall_ts
    qint64 _last_offline_received = -1;
    qint64 _max_received_timestamp = 0;
    QTimer *_cleanup_timer;
    static constexpr int MESSAGE_TIMEOUT_SEC = 30;
    static constexpr int HISTORY_PAGE_SIZE = 50;
};

#endif

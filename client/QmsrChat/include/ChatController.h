#pragma once
/**
 * @file ChatController.h
 * @brief QML 与 C++ 业务层通信桥梁（Phase 5B.6 精简版）
 * @details 封装聊天业务逻辑，暴露属性和方法供 QML 调用。
 *          文件/图片逻辑委托给 FileCoordinator，右键菜单委托给 MessageActions。
 */
#ifndef CHATCONTROLLER_H
#define CHATCONTROLLER_H

#include "ChatListModel.h"
#include "DbWorker.h"
#include "FileCoordinator.h"
#include "MessageActions.h"
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
    qint64 send_time = 0;
};

class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentUid READ GetCurrentUid NOTIFY sigCurrentUidChanged)
    Q_PROPERTY(int targetUid READ GetTargetUid WRITE setTargetUid NOTIFY sigTargetUidChanged)
    Q_PROPERTY(bool isConnected READ IsConnected NOTIFY sigConnectionStatusChanged)

public:
    explicit ChatController(QObject *parent = nullptr);
    ~ChatController();

    // ── 文本消息 ──
    Q_INVOKABLE void sendMessage(const QString &content);

    // ── 文件/图片（委托给 FileCoordinator） ──
    Q_INVOKABLE void sendFile(const QString &filePath);
    Q_INVOKABLE void sendImage(const QString &imagePath, const QString &caption);
    Q_INVOKABLE void openImageViewer(const QString &imageId);
    Q_INVOKABLE QVariantList getImageListForViewer() const;

    // ── 右键菜单（委托给 MessageActions） ──
    Q_INVOKABLE void actionReply(qint64 timestamp);
    Q_INVOKABLE void actionCopyText(qint64 timestamp);
    Q_INVOKABLE void actionRecall(qint64 timestamp);
    Q_INVOKABLE void actionEdit(qint64 timestamp, const QString &newContent);
    Q_INVOKABLE void actionDelete(qint64 timestamp);

    Q_INVOKABLE void setTargetUid(int uid);
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void loadMoreHistory();
    Q_INVOKABLE void initialize();

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
    void sigShowImageViewer(QVariantList imageList, int currentIndex);
    void sigSendRecallMsg(const ChatRecallMsgStruct &msg);
    void sigSendEditMsg(const ChatEditMsgStruct &msg);
    void sigSetReplyContext(const QString &prefix);

public slots:
    void slotOnChatTextMsg(const ChatTextMsgStruct &msg);
    void slotOnChatAck(const ChatAckStruct &ack);
    void slotOnConnectionStateChanged(bool connected);
    void slotOnOfflineProgress(const OfflineAckStruct &ack);
    void slotOnReconnected();
    void slotOnChatLoginRsp(const ChatLoginRspStruct &rsp);
    void FlushPendingRecalls();
    void slotOnHistoryLoaded(const QVector<ChatMessage> &messages);
    void slotOnMessageSaved(bool success);
    void slotCleanTimeoutMessages();
    void slotOnChatRecallRsp(const ChatEditAckStruct &ack);
    void slotOnChatEditAck(const ChatEditAckStruct &ack);
    void slotOnChatRecallNotify(const ChatRecallNotifyStruct &n);
    void slotOnChatEditNotify(const ChatEditNotifyStruct &n);

private:
    void ConnectSignals();
    void DisconnectSignals();

    int _target_uid;
    int _current_uid;
    bool _is_connected;
    ChatListModel *_chat_model = nullptr;
    QMutex _pending_mutex;
    QHash<QString, PendingMessageInfo> _pending_messages;
    QHash<qint64, qint64> _pending_recall;
    qint64 _last_offline_received = -1;
    qint64 _max_received_timestamp = 0;
    QTimer *_cleanup_timer;
    static constexpr int MESSAGE_TIMEOUT_SEC = 30;
    static constexpr int HISTORY_PAGE_SIZE = 50;

    // ── Phase 5B.6 子组件 ──
    MessageActions *_msg_actions;
    FileCoordinator *_file_coord;
};

#endif

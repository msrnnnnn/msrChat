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

    Q_INVOKABLE void sendMessage(const QString &content);
    Q_INVOKABLE void sendFile(const QString &filePath);
    Q_INVOKABLE void sendImage(const QString &imagePath, const QString &caption);
    Q_INVOKABLE void setTargetUid(int uid);
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void loadMoreHistory();
    Q_PROPERTY(bool hasMoreHistory READ hasMoreHistory NOTIFY sigHasMoreHistoryChanged)
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void searchMessages(const QString &keyword);
    Q_INVOKABLE void initialize();
    Q_INVOKABLE void openImageViewer(const QString &imageId);
    Q_INVOKABLE QVariantList getImageListForViewer() const;

    // Phase 6 — 右键菜单 6 项 action
    Q_INVOKABLE void actionReply(qint64 timestamp);
    Q_INVOKABLE void actionCopyText(qint64 timestamp);
    Q_INVOKABLE void actionRecall(qint64 timestamp);
    Q_INVOKABLE void actionEdit(qint64 timestamp, const QString &newContent);
    Q_INVOKABLE void actionSaveAs(qint64 timestamp);
    Q_INVOKABLE void actionDelete(qint64 timestamp);

    void drainBufferedMessages(const QVector<ChatTextMsgStruct> &msgs);

    int GetCurrentUid() const;
    int GetTargetUid() const;
    bool IsConnected() const;
    bool hasMoreHistory() const { return _has_more_history; }
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
    void sigFileRecvComplete(int64_t task_id, const QString &filepath, bool success, const QString &error);
    void sigHistoryCleared();
    void sigHasMoreHistoryChanged();
    void sigSendImageMsg(const ChatImageStruct &msg);
    void sigSendEditMsg(const ChatEditMsgStruct &msg);
    void sigShowImageViewer(QVariantList imageList, int currentIndex);
    // Phase 6 — 菜单 action signals
    void sigSendRecallMsg(const ChatRecallMsgStruct &msg);
    void sigSetReplyContext(const QString &prefix);
    void sigShowSaveAsDialog(const QString &imagePath);

public slots:
    void slotOnChatTextMsg(const ChatTextMsgStruct &msg);
    void slotOnChatAck(const ChatAckStruct &ack);
    void slotOnConnectionStateChanged(bool connected);
    void slotOnOfflineProgress(const OfflineAckStruct &ack);
    void slotOnReconnected();
    void slotOnChatLoginRsp(const ChatLoginRspStruct &rsp);
    void slotOnHistoryLoaded(const QVector<ChatMessage> &messages);
    void slotOnMessageSaved(bool success);
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
    QVariantMap ChatMessageToVariant(const ChatMessage &msg);

    int _target_uid;
    int _current_uid;
    bool _is_connected;
    ChatListModel *_chat_model = nullptr;
    QMutex _pending_mutex;
    QHash<QString, PendingMessageInfo> _pending_messages;
    qint64 _last_offline_received = -1;
    qint64 _max_received_timestamp = 0;
    bool _has_more_history = true;
    QTimer *_cleanup_timer;
    static constexpr int MESSAGE_TIMEOUT_SEC = 30;
    static constexpr int HISTORY_PAGE_SIZE = 50;
};

#endif

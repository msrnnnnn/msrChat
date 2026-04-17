/**
 * @file ChatController.h
 * @brief QML 与 C++ 业务层通信桥梁
 * @details 封装聊天业务逻辑，暴露属性和方法供 QML 调用，监听网络信号并转发给 QML。
 */
#ifndef CHATCONTROLLER_H
#define CHATCONTROLLER_H

#include "DbWorker.h"
#include "ProtocolStructs.h"
#include "TcpMgr.h"
#include "UserMgr.h"
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUuid>

struct PendingMessageInfo
{
    QString content;
    qint64 send_time;
    qint64 msg_id;
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
    Q_INVOKABLE void setTargetUid(int uid);
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void clearHistory();

    int GetCurrentUid() const;
    int GetTargetUid() const;
    bool IsConnected() const;

    Q_INVOKABLE void initialize();

signals:
    void sigCurrentUidChanged();
    void sigTargetUidChanged();
    void sigConnectionStatusChanged();
    void sigMessageReceived(const QVariantMap &msgData);
    void sigMessageSent(const QVariantMap &msgData);
    void sigMessageStatusChanged(const QString &clientMsgId, int status);
    void sigHistoryLoaded(const QVariantList &messages);
    void sigError(const QString &error);

private slots:
    void slotOnChatTextMsg(const ChatTextMsgStruct &msg);
    void slotOnChatAck(const ChatAckStruct &ack);
    void slotOnOfflineAck(const OfflineAckStruct &ack);
    void slotOnReconnected();
    void slotOnHistoryLoaded(const QVector<ChatMessage> &messages);
    void slotOnMessageSaved(bool success);
    void slotCleanTimeoutMessages();

private:
    void ConnectSignals();
    void DisconnectSignals();
    void AddMessageToModel(const ChatMessage &msg);
    QVariantMap ChatMessageToVariant(const ChatMessage &msg);

    int _target_uid;
    int _current_uid;
    bool _is_connected;
    QSet<QString> _received_msg_ids;
    QHash<QString, PendingMessageInfo> _pending_messages;
    QTimer *_cleanup_timer;
    static constexpr int MESSAGE_TIMEOUT_SEC = 30;
    static constexpr int HISTORY_PAGE_SIZE = 50;
};

#endif

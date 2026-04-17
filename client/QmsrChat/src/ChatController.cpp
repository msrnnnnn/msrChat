/**
 * @file ChatController.cpp
 * @brief ChatController 实现
 */
#include "ChatController.h"
#include "ChatListModel.h"
#include <QDebug>
#include <QTimer>

ChatController::ChatController(QObject *parent)
    : QObject(parent),
      _target_uid(0),
      _current_uid(0),
      _is_connected(false)
{
    _cleanup_timer = new QTimer(this);
    connect(_cleanup_timer, &QTimer::timeout, this, &ChatController::slotCleanTimeoutMessages);
    _cleanup_timer->start(10000);

    ConnectSignals();
}

ChatController::~ChatController()
{
    DisconnectSignals();
    _cleanup_timer->stop();
}

void ChatController::initialize()
{
    _current_uid = UserMgr::Instance()->GetUid();
    _is_connected = true;
    emit sigCurrentUidChanged();
    emit sigConnectionStatusChanged();
}

void ChatController::ConnectSignals()
{
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_chat_text_msg, this, &ChatController::slotOnChatTextMsg, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sig_chat_ack, this, &ChatController::slotOnChatAck, Qt::QueuedConnection);
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_offline_ack, this, &ChatController::slotOnOfflineAck, Qt::QueuedConnection);
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_reconnected, this, &ChatController::slotOnReconnected, Qt::QueuedConnection);
}

void ChatController::DisconnectSignals()
{
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_text_msg, this, &ChatController::slotOnChatTextMsg);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_ack, this, &ChatController::slotOnChatAck);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_offline_ack, this, &ChatController::slotOnOfflineAck);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_reconnected, this, &ChatController::slotOnReconnected);
}

int ChatController::GetCurrentUid() const
{
    return _current_uid;
}

int ChatController::GetTargetUid() const
{
    return _target_uid;
}

void ChatController::setTargetUid(int uid)
{
    if (_target_uid != uid)
    {
        _target_uid = uid;
        emit sigTargetUidChanged();
        loadHistory();
    }
}

bool ChatController::IsConnected() const
{
    return _is_connected;
}

void ChatController::sendMessage(const QString &content)
{
    if (_target_uid <= 0)
    {
        qWarning() << "[ChatController] Invalid target UID";
        emit sigError(QStringLiteral("目标用户无效"));
        return;
    }

    if (content.isEmpty())
    {
        qWarning() << "[ChatController] Empty message content";
        return;
    }

    if (content.size() > 512)
    {
        qWarning() << "[ChatController] Message content too long";
        emit sigError(QStringLiteral("内容过长"));
        return;
    }

    QString client_msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ChatTextReqStruct req;
    req.from_uid = _current_uid;
    req.to_uid = _target_uid;
    req.content = content;
    req.client_msg_id = client_msg_id;

    PendingMessageInfo info;
    info.content = content;
    info.send_time = QDateTime::currentSecsSinceEpoch();
    info.msg_id = qHash(client_msg_id);
    _pending_messages.insert(client_msg_id, info);

    ChatMessage msg;
    msg.id = info.msg_id;
    msg.from_uid = _current_uid;
    msg.to_uid = _target_uid;
    msg.content = content;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    msg.status = 0;

    emit sigMessageSent(ChatMessageToVariant(msg));

    DbThreadPool::Instance().SaveMessage(msg);
    TcpMgr::Instance()->slot_send_chat_text_req(req);

    qDebug() << "[ChatController] Message sent, client_msg_id:" << client_msg_id;
}

void ChatController::loadHistory()
{
    if (_current_uid <= 0 || _target_uid <= 0)
    {
        qWarning() << "[ChatController] Cannot load history: invalid UIDs";
        return;
    }

    connect(
        &DbThreadPool::Instance(), &DbThreadPool::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded,
        Qt::UniqueConnection);

    DbThreadPool::Instance().GetMessages(_current_uid, _target_uid, LLONG_MAX, HISTORY_PAGE_SIZE);
}

void ChatController::clearHistory()
{
}

void ChatController::slotOnChatTextMsg(const ChatTextMsgStruct &msg)
{
    QString dedup_key;
    if (!msg.client_msg_id.isEmpty())
    {
        dedup_key = msg.client_msg_id;
    }
    else if (msg.server_msg_id > 0)
    {
        dedup_key = QString::number(msg.server_msg_id);
    }

    if (!dedup_key.isEmpty())
    {
        if (_received_msg_ids.contains(dedup_key))
        {
            qDebug() << "[ChatController] Duplicate message dropped:" << dedup_key;
            return;
        }
        _received_msg_ids.insert(dedup_key);
        if (_received_msg_ids.size() > 10000)
        {
            QList<QString> values = _received_msg_ids.values();
            _received_msg_ids = QSet<QString>(values.begin() + values.size() / 2, values.end());
        }
    }

    ChatMessage chatMsg;
    chatMsg.id = msg.server_msg_id > 0 ? msg.server_msg_id : qHash(dedup_key);
    chatMsg.from_uid = msg.from_uid;
    chatMsg.to_uid = msg.to_uid;
    chatMsg.content = msg.content;
    chatMsg.timestamp = msg.timestamp;
    chatMsg.status = 1;

    emit sigMessageReceived(ChatMessageToVariant(chatMsg));

    DbThreadPool::Instance().SaveMessage(chatMsg);
}

void ChatController::slotOnChatAck(const ChatAckStruct &ack)
{
    if (ack.client_msg_id.isEmpty())
    {
        return;
    }

    PendingMessageInfo info = _pending_messages.take(ack.client_msg_id);
    if (info.content.isEmpty())
    {
        return;
    }

    int status = ack.error == 0 ? 1 : -1;
    emit sigMessageStatusChanged(ack.client_msg_id, status);

    if (ack.error != 0)
    {
        QString errorMsg = ack.message.isEmpty() ? QStringLiteral("发送失败") : ack.message;
        emit sigError(errorMsg);
    }
}

void ChatController::slotOnOfflineAck(const OfflineAckStruct &ack)
{
    _current_uid = UserMgr::Instance()->GetUid();

    OfflineAckReqStruct req;
    req.received = ack.received;
    TcpMgr::Instance()->slot_send_offline_ack_req(req);
}

void ChatController::slotOnReconnected()
{
    _is_connected = true;
    emit sigConnectionStatusChanged();
    emit sigError(QStringLiteral("网络已重连"));
}

void ChatController::slotOnHistoryLoaded(const QVector<ChatMessage> &messages)
{
    disconnect(
        &DbThreadPool::Instance(), &DbThreadPool::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded);

    QVariantList msgList;
    for (const auto &msg : messages)
    {
        msgList.append(ChatMessageToVariant(msg));
    }
    emit sigHistoryLoaded(msgList);
}

void ChatController::slotOnMessageSaved(bool success)
{
    if (!success)
    {
        qWarning() << "[ChatController] Failed to save message to database";
    }
}

void ChatController::slotCleanTimeoutMessages()
{
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 threshold = now - MESSAGE_TIMEOUT_SEC;

    QStringList expired_ids;
    for (auto it = _pending_messages.begin(); it != _pending_messages.end(); ++it)
    {
        if (it.value().send_time < threshold)
        {
            expired_ids.append(it.key());
        }
    }

    for (const QString &id : expired_ids)
    {
        _pending_messages.take(id);
        emit sigMessageStatusChanged(id, -1);
        qDebug() << "[ChatController] Message timeout, client_msg_id:" << id;
    }
}

QVariantMap ChatController::ChatMessageToVariant(const ChatMessage &msg)
{
    QVariantMap map;
    map["id"] = msg.id;
    map["fromUid"] = msg.from_uid;
    map["toUid"] = msg.to_uid;
    map["content"] = msg.content;
    map["timestamp"] = msg.timestamp;
    map["status"] = msg.status;
    map["isSelf"] = msg.from_uid == _current_uid;
    return map;
}

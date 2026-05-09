/**
 * @file ChatController.cpp
 * @brief ChatController 实现
 */
#include "ChatController.h"
#include "FileSendMgr.h"
#include <QDebug>
#include <QFileInfo>
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
    _is_connected = TcpMgr::Instance()->IsConnected();
    emit sigCurrentUidChanged();
    emit sigConnectionStatusChanged();
}

void ChatController::setChatModel(ChatListModel *model)
{
    _chat_model = model;
    if (_chat_model != nullptr)
    {
        _chat_model->SetCurrentUid(_current_uid);
    }
}

void ChatController::ConnectSignals()
{
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_chat_text_msg, this, &ChatController::slotOnChatTextMsg, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sig_chat_ack, this, &ChatController::slotOnChatAck, Qt::QueuedConnection);
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_con_success, this, &ChatController::slotOnConnectionStateChanged,
        Qt::QueuedConnection);
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_offline_ack, this, &ChatController::slotOnOfflineProgress,
        Qt::QueuedConnection);
    connect(
        TcpMgr::Instance(), &TcpMgr::sig_reconnected, this, &ChatController::slotOnReconnected, Qt::QueuedConnection);
    connect(
        &DbThreadPool::Instance(), &DbThreadPool::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded,
        Qt::QueuedConnection);
    connect(
        &DbThreadPool::Instance(), &DbThreadPool::sig_messages_saved, this, &ChatController::slotOnMessageSaved,
        Qt::QueuedConnection);
    connect(
        &FileSendMgr::Instance(), &FileSendMgr::sigSendProgress, this,
        [this](int64_t task_id, int progress, int64_t sent, int64_t total)
        { emit sigFileSendProgress(task_id, progress, sent, total); }, Qt::QueuedConnection);
    connect(
        &FileSendMgr::Instance(), &FileSendMgr::sigSendComplete, this,
        [this](int64_t task_id, bool success, const QString &error)
        { emit sigFileSendComplete(task_id, success, error); }, Qt::QueuedConnection);
}

void ChatController::DisconnectSignals()
{
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_text_msg, this, &ChatController::slotOnChatTextMsg);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_ack, this, &ChatController::slotOnChatAck);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_con_success, this, &ChatController::slotOnConnectionStateChanged);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_offline_ack, this, &ChatController::slotOnOfflineProgress);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_reconnected, this, &ChatController::slotOnReconnected);
    disconnect(
        &DbThreadPool::Instance(), &DbThreadPool::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded);
    disconnect(&DbThreadPool::Instance(), &DbThreadPool::sig_messages_saved, this, &ChatController::slotOnMessageSaved);
    disconnect(&FileSendMgr::Instance(), nullptr, this, nullptr);
}

int ChatController::GetCurrentUid() const
{
    return _current_uid;
}

int ChatController::GetTargetUid() const
{
    return _target_uid;
}

bool ChatController::IsConnected() const
{
    return _is_connected;
}

void ChatController::setTargetUid(int uid)
{
    if (_target_uid == uid)
    {
        return;
    }

    _target_uid = uid;
    emit sigTargetUidChanged();

    if (_chat_model != nullptr)
    {
        _chat_model->ClearMessages();
    }
    loadHistory();
}

void ChatController::sendMessage(const QString &content)
{
    if (_target_uid <= 0)
    {
        emit sigError(QStringLiteral("目标用户无效"));
        return;
    }

    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty())
    {
        return;
    }

    if (trimmed.size() > 512)
    {
        emit sigError(QStringLiteral("内容过长"));
        return;
    }

    const QString client_msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ChatMessage msg;
    msg.client_msg_id = client_msg_id;
    msg.from_uid = _current_uid;
    msg.to_uid = _target_uid;
    msg.content = trimmed;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    msg.status = 0;

    _pending_messages.insert(client_msg_id, PendingMessageInfo{QDateTime::currentSecsSinceEpoch()});
    if (_chat_model != nullptr)
    {
        _chat_model->AddMessage(msg);
    }

    DbThreadPool::Instance().SaveMessage(msg);

    ChatTextReqStruct req;
    req.from_uid = msg.from_uid;
    req.to_uid = msg.to_uid;
    req.content = msg.content;
    req.client_msg_id = msg.client_msg_id;
    TcpMgr::Instance()->slot_send_chat_text_req(req);
}

void ChatController::sendFile(const QString &filePath)
{
    Q_UNUSED(filePath);
    emit sigError(QStringLiteral("文件传输暂不可用"));
}

void ChatController::loadHistory()
{
    if (_current_uid <= 0 || _target_uid <= 0)
    {
        return;
    }

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

    if (!dedup_key.isEmpty() && _received_msg_ids.contains(dedup_key))
    {
        return;
    }
    if (!dedup_key.isEmpty())
    {
        _received_msg_ids.insert(dedup_key);
    }

    ChatMessage chat_msg;
    chat_msg.client_msg_id = msg.client_msg_id;
    chat_msg.server_msg_id = msg.server_msg_id;
    chat_msg.from_uid = msg.from_uid;
    chat_msg.to_uid = msg.to_uid;
    chat_msg.content = msg.content;
    chat_msg.timestamp = msg.timestamp;
    chat_msg.status = 1;

    DbThreadPool::Instance().SaveMessage(chat_msg);
    if (_chat_model != nullptr && ((chat_msg.from_uid == _target_uid && chat_msg.to_uid == _current_uid) ||
                                   (chat_msg.from_uid == _current_uid && chat_msg.to_uid == _target_uid)))
    {
        _chat_model->UpsertMessage(chat_msg);
    }
}

void ChatController::slotOnChatAck(const ChatAckStruct &ack)
{
    if (ack.client_msg_id.isEmpty() || !_pending_messages.contains(ack.client_msg_id))
    {
        return;
    }

    _pending_messages.remove(ack.client_msg_id);

    int status = 1;
    if (ack.error != 0)
    {
        status = -1;
    }
    else if (ack.message == QStringLiteral("stored"))
    {
        status = 2;
    }

    DbThreadPool::Instance().UpdateMessageStatus(ack.client_msg_id, status);
    if (_chat_model != nullptr)
    {
        _chat_model->UpdateMessageStatus(ack.client_msg_id, status);
    }

    if (ack.error != 0)
    {
        emit sigError(ack.message.isEmpty() ? QStringLiteral("发送失败") : ack.message);
    }
}

void ChatController::slotOnOfflineProgress(const OfflineAckStruct &ack)
{
    if (ack.received <= _last_offline_received)
    {
        return;
    }
    _last_offline_received = ack.received;

    OfflineAckReqStruct req;
    req.received = ack.received;
    TcpMgr::Instance()->slot_send_offline_ack_req(req);
}

void ChatController::slotOnConnectionStateChanged(bool connected)
{
    if (_is_connected == connected)
    {
        return;
    }

    _is_connected = connected;
    emit sigConnectionStatusChanged();
}

void ChatController::slotOnReconnected()
{
    ChatLoginReqStruct req;
    req.uid = UserMgr::Instance()->GetUid();
    req.token = UserMgr::Instance()->GetToken();
    TcpMgr::Instance()->slot_send_chat_login_req(req);
}

void ChatController::slotOnHistoryLoaded(const QVector<ChatMessage> &messages)
{
    if (_chat_model != nullptr)
    {
        _chat_model->SetMessages(messages);
    }
}

void ChatController::slotOnMessageSaved(bool success)
{
    if (!success)
    {
        emit sigError(QStringLiteral("消息保存失败"));
    }
}

void ChatController::slotCleanTimeoutMessages()
{
    const qint64 threshold = QDateTime::currentSecsSinceEpoch() - MESSAGE_TIMEOUT_SEC;
    QStringList expired_ids;

    for (auto it = _pending_messages.cbegin(); it != _pending_messages.cend(); ++it)
    {
        if (it.value().send_time < threshold)
        {
            expired_ids.append(it.key());
        }
    }

    for (const QString &id : expired_ids)
    {
        _pending_messages.remove(id);
        DbThreadPool::Instance().UpdateMessageStatus(id, -1);
        if (_chat_model != nullptr)
        {
            _chat_model->UpdateMessageStatus(id, -1);
        }
    }
}

QVariantMap ChatController::ChatMessageToVariant(const ChatMessage &msg)
{
    QVariantMap map;
    map["id"] = msg.id;
    map["clientMsgId"] = msg.client_msg_id;
    map["serverMsgId"] = msg.server_msg_id;
    map["fromUid"] = msg.from_uid;
    map["toUid"] = msg.to_uid;
    map["content"] = msg.content;
    map["timestamp"] = msg.timestamp;
    map["status"] = msg.status;
    map["isSelf"] = msg.from_uid == _current_uid;
    return map;
}

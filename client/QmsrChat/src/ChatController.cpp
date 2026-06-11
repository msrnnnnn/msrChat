/**
 * @file ChatController.cpp
 * @brief ChatController 实现（Phase 5B.6 精简版）
 * @details 文本消息 + 连接状态 + 历史加载。文件/图片委托给 FileCoordinator，
 *          右键菜单委托给 MessageActions。
 */
#include "ChatController.h"
#include "ImageDownloadMgr.h"
#include <QDebug>
#include <QTimer>

ChatController::ChatController(QObject *parent)
    : QObject(parent),
      _target_uid(0),
      _current_uid(0),
      _is_connected(false)
{
    _msg_actions = new MessageActions(this);
    _file_coord = new FileCoordinator(this);

    _cleanup_timer = new QTimer(this);
    connect(_cleanup_timer, &QTimer::timeout, this, &ChatController::slotCleanTimeoutMessages);
    _cleanup_timer->start(10000);
    ConnectSignals();

    // 桥接 MessageActions 信号
    connect(_msg_actions, &MessageActions::sigSendRecallMsg,
            this, &ChatController::sigSendRecallMsg);
    connect(_msg_actions, &MessageActions::sigSendEditMsg,
            this, &ChatController::sigSendEditMsg);
    connect(_msg_actions, &MessageActions::sigSetReplyContext,
            this, &ChatController::sigSetReplyContext);

    // 桥接 FileCoordinator 信号
    connect(_file_coord, &FileCoordinator::sigError,
            this, &ChatController::sigError);
    connect(_file_coord, &FileCoordinator::sigFileSendStarted,
            this, &ChatController::sigFileSendStarted);
    connect(_file_coord, &FileCoordinator::sigFileSendProgress,
            this, &ChatController::sigFileSendProgress);
    connect(_file_coord, &FileCoordinator::sigFileSendComplete,
            this, &ChatController::sigFileSendComplete);
    connect(_file_coord, &FileCoordinator::sigFileRecvProgress,
            this, &ChatController::sigFileRecvProgress);
    connect(_file_coord, &FileCoordinator::sigFileRecvStarted,
            this, &ChatController::sigFileRecvStarted);
    connect(_file_coord, &FileCoordinator::sigFileRecvComplete,
            this, &ChatController::sigFileRecvComplete);
    connect(_file_coord, &FileCoordinator::sigShowImageViewer,
            this, &ChatController::sigShowImageViewer);
}

ChatController::~ChatController()
{
    DisconnectSignals();
    _file_coord->disconnectSignals();
    _cleanup_timer->stop();
}

// ── 委托方法 ──

void ChatController::sendFile(const QString &filePath)
{
    _file_coord->sendFile(filePath);
}

void ChatController::sendImage(const QString &imagePath, const QString &caption)
{
    _file_coord->sendImage(imagePath, caption);
}

void ChatController::openImageViewer(const QString &imageId)
{
    _file_coord->openImageViewer(imageId);
}

QVariantList ChatController::getImageListForViewer() const
{
    return _file_coord->getImageListForViewer();
}

void ChatController::actionReply(qint64 timestamp) { _msg_actions->actionReply(timestamp); }
void ChatController::actionCopyText(qint64 timestamp) { _msg_actions->actionCopyText(timestamp); }
void ChatController::actionRecall(qint64 timestamp) { _msg_actions->actionRecall(timestamp); }
void ChatController::actionEdit(qint64 timestamp, const QString &newContent) { _msg_actions->actionEdit(timestamp, newContent); }
void ChatController::actionDelete(qint64 timestamp) { _msg_actions->actionDelete(timestamp); }

// ── 初始化与属性 ──

void ChatController::initialize()
{
    _current_uid = UserMgr::Instance()->GetUid();
    _is_connected = TcpMgr::Instance()->IsConnected();
    qDebug() << "[ChatController::initialize] _current_uid =" << _current_uid
             << "_is_connected =" << _is_connected
             << "_chat_model =" << (_chat_model != nullptr);

    if (_chat_model != nullptr && _current_uid > 0)
        _chat_model->SetCurrentUid(_current_uid);

    // 同步子组件状态
    _msg_actions->setCurrentUid(_current_uid);
    _file_coord->setCurrentUid(_current_uid);

    emit sigCurrentUidChanged();
    emit sigConnectionStatusChanged();
}

void ChatController::setChatModel(ChatListModel *model)
{
    _chat_model = model;
    if (_chat_model != nullptr)
        _chat_model->SetCurrentUid(_current_uid);

    _msg_actions->setChatModel(model);
    _file_coord->setChatModel(model);
}

void ChatController::setTargetUid(int uid)
{
    if (_target_uid == uid) return;

    _target_uid = uid;
    _max_received_timestamp = 0;
    _file_coord->setTargetUid(uid);
    _file_coord->setMaxReceivedTimestamp(&_max_received_timestamp);
    emit sigTargetUidChanged();

    if (_chat_model != nullptr)
        _chat_model->ClearMessages();
    loadHistory();
    FlushPendingRecalls();
}

int ChatController::GetCurrentUid() const { return _current_uid; }
int ChatController::GetTargetUid() const { return _target_uid; }
bool ChatController::IsConnected() const { return _is_connected; }

// ── 信号槽连接 ──

void ChatController::ConnectSignals()
{
    // TcpMgr 文本/状态信号
    connect(TcpMgr::Instance(), &TcpMgr::sigChatTextMsg,
            this, &ChatController::slotOnChatTextMsg, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatAck,
            this, &ChatController::slotOnChatAck, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigConSuccess,
            this, &ChatController::slotOnConnectionStateChanged, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigOfflineAck,
            this, &ChatController::slotOnOfflineProgress, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigReconnected,
            this, &ChatController::slotOnReconnected, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatLoginRsp,
            this, &ChatController::slotOnChatLoginRsp, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatRecallRsp,
            this, &ChatController::slotOnChatRecallRsp, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatEditAck,
            this, &ChatController::slotOnChatEditAck, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatRecallNotify,
            this, &ChatController::slotOnChatRecallNotify, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatEditNotify,
            this, &ChatController::slotOnChatEditNotify, Qt::QueuedConnection);

    // DB 信号
    connect(&DbThreadManager::Instance(), &DbThreadManager::sigMessagesLoaded,
            this, &ChatController::slotOnHistoryLoaded, Qt::QueuedConnection);
    connect(&DbThreadManager::Instance(), &DbThreadManager::sigMessagesSaved,
            this, &ChatController::slotOnMessageSaved, Qt::QueuedConnection);

    // 撤回/编辑发送桥接
    connect(this, &ChatController::sigSendRecallMsg,
            TcpMgr::Instance(), &TcpMgr::slot_send_chat_recall, Qt::QueuedConnection);
    connect(this, &ChatController::sigSendEditMsg,
            TcpMgr::Instance(), &TcpMgr::slot_send_chat_edit, Qt::QueuedConnection);

    // FileCoordinator 信号连接
    _file_coord->connectSignals();
}

void ChatController::DisconnectSignals()
{
    disconnect(TcpMgr::Instance(), nullptr, this, nullptr);
    disconnect(&DbThreadManager::Instance(), nullptr, this, nullptr);
    disconnect(this, &ChatController::sigSendRecallMsg, TcpMgr::Instance(), &TcpMgr::slot_send_chat_recall);
    disconnect(this, &ChatController::sigSendEditMsg, TcpMgr::Instance(), &TcpMgr::slot_send_chat_edit);
    _file_coord->disconnectSignals();
}

// ── 文本消息处理 ──

void ChatController::sendMessage(const QString &content)
{
    if (_target_uid <= 0) { emit sigError(QStringLiteral("目标用户无效")); return; }

    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty()) return;
    if (trimmed.size() > 4096) { emit sigError(QStringLiteral("内容过长")); return; }

    const QString client_msg_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ChatMessage msg;
    msg.client_msg_id = client_msg_id;
    msg.from_uid = _current_uid;
    msg.to_uid = _target_uid;
    msg.content = trimmed;
    msg.timestamp = qMax(QDateTime::currentMSecsSinceEpoch(), _max_received_timestamp + 1);
    msg.status = 0;

    QMutexLocker locker(&_pending_mutex);
    _pending_messages.insert(client_msg_id, PendingMessageInfo{QDateTime::currentSecsSinceEpoch()});
    if (_chat_model != nullptr)
        _chat_model->AddMessage(msg);

    DbThreadManager::Instance().SaveMessage(msg);

    ChatTextReqStruct req;
    req.from_uid = msg.from_uid;
    req.to_uid = msg.to_uid;
    req.content = msg.content;
    req.client_msg_id = msg.client_msg_id;
    req.timestamp = msg.timestamp;
    TcpMgr::Instance()->slot_send_chat_text_req(req);
}

void ChatController::slotOnChatTextMsg(const ChatTextMsgStruct &msg)
{
    ChatMessage chat_msg;
    chat_msg.client_msg_id = msg.client_msg_id;
    chat_msg.server_msg_id = msg.server_msg_id;
    chat_msg.from_uid = msg.from_uid;
    chat_msg.to_uid = msg.to_uid;
    chat_msg.content = msg.content;
    chat_msg.timestamp = msg.timestamp;
    _max_received_timestamp = qMax(_max_received_timestamp, msg.timestamp);
    chat_msg.status = 1;

    DbThreadManager::Instance().SaveMessage(chat_msg);

    if (_chat_model != nullptr &&
       ((chat_msg.from_uid == _target_uid && chat_msg.to_uid == _current_uid) ||
        (chat_msg.from_uid == _current_uid && chat_msg.to_uid == _target_uid)))
    {
        _chat_model->UpsertMessage(chat_msg);
        FlushPendingRecalls();
    }
}

void ChatController::slotOnChatAck(const ChatAckStruct &ack)
{
    QMutexLocker locker(&_pending_mutex);
    if (ack.client_msg_id.isEmpty() || !_pending_messages.contains(ack.client_msg_id)) return;

    _pending_messages.remove(ack.client_msg_id);
    int status = 1;
    if (ack.error != 0) status = -1;
    else if (ack.message == QStringLiteral("stored")) status = 2;

    DbThreadManager::Instance().UpdateMessageStatus(ack.client_msg_id, status);
    if (_chat_model != nullptr)
        _chat_model->UpdateMessageStatus(ack.client_msg_id, status);

    if (ack.error != 0)
        emit sigError(ack.message.isEmpty() ? QStringLiteral("发送失败") : ack.message);
}

void ChatController::drainBufferedMessages(const QVector<ChatTextMsgStruct> &msgs)
{
    for (const ChatTextMsgStruct &msg : msgs)
        slotOnChatTextMsg(msg);
}

// ── 连接状态 ──

void ChatController::slotOnConnectionStateChanged(bool connected)
{
    if (_is_connected == connected) return;
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

void ChatController::slotOnChatLoginRsp(const ChatLoginRspStruct &rsp)
{
    if (rsp.error != 0) {
        qWarning() << "ChatController: chat login re-auth failed, error:" << rsp.error;
        _is_connected = false;
        emit sigConnectionStatusChanged();
        emit sigError(rsp.message.isEmpty() ? QStringLiteral("聊天会话恢复失败") : rsp.message);
        return;
    }
    if (!_is_connected) {
        _is_connected = true;
        emit sigConnectionStatusChanged();
    }
}

void ChatController::slotOnOfflineProgress(const OfflineAckStruct &ack)
{
    if (ack.received <= _last_offline_received) return;
    _last_offline_received = ack.received;
    if (ack.received < ack.total) {
        OfflineAckReqStruct req;
        req.received = ack.received;
        TcpMgr::Instance()->slot_send_offline_ack_req(req);
    }
}

// ── 历史消息 ──

void ChatController::loadHistory()
{
    int currentUid = UserMgr::Instance()->GetUid();
    if (currentUid <= 0 || _target_uid <= 0) return;
    DbThreadManager::Instance().GetMessages(currentUid, _target_uid, LLONG_MAX, HISTORY_PAGE_SIZE);
}

void ChatController::loadMoreHistory()
{
    int currentUid = UserMgr::Instance()->GetUid();
    if (currentUid <= 0 || _target_uid <= 0) return;
    qint64 before_time = LLONG_MAX;
    if (_chat_model != nullptr && _chat_model->rowCount() > 0)
        before_time = _chat_model->GetEarliestTimestamp();
    DbThreadManager::Instance().GetMessages(currentUid, _target_uid, before_time, HISTORY_PAGE_SIZE);
}

void ChatController::slotOnHistoryLoaded(const QVector<ChatMessage> &messages)
{
    if (_chat_model != nullptr)
        _chat_model->PrependMessages(messages);

    // 历史图片消息缓存检查
    for (const auto &msg : messages) {
        if (msg.type == 1 && msg.image_path.isEmpty() && !msg.image_id.isEmpty()) {
            if (ImageDownloadMgr::Instance().IsCached(msg.image_id)) {
                QString cachePath = ImageDownloadMgr::Instance().GetCachePath(msg.image_id, msg.image_ext);
                if (_chat_model)
                    _chat_model->UpdateImagePath(msg.image_id, cachePath);
            } else {
                ImageDownloadMgr::Instance().Request(msg.image_id, 0, msg.image_ext);
            }
        }
    }

    FlushPendingRecalls();
}

void ChatController::slotOnMessageSaved(bool success)
{
    if (!success) emit sigError(QStringLiteral("消息保存失败"));
}

// ── 撤回/编辑通知 ──

void ChatController::slotOnChatRecallRsp(const ChatEditAckStruct &ack)
{
    if (ack.error != 0) {
        qWarning() << "[ChatController] recall failed: error=" << ack.error;
        emit sigError(QStringLiteral("Recall failed (error %1)").arg(ack.error));
        return;
    }
    if (_chat_model)
        _chat_model->MarkRecalled(ack.msg_timestamp, _current_uid);
}

void ChatController::slotOnChatEditAck(const ChatEditAckStruct &ack)
{
    if (ack.error != 0) {
        qWarning() << "[ChatController] edit failed: error=" << ack.error;
        emit sigError(QStringLiteral("Edit failed (error %1)").arg(ack.error));
        return;
    }
    if (_chat_model && !ack.new_content.isEmpty())
        _chat_model->MarkEdited(ack.msg_timestamp, ack.new_content, ack.edit_ts);
}

void ChatController::slotOnChatRecallNotify(const ChatRecallNotifyStruct &n)
{
    QMutexLocker lock(&_pending_mutex);
    _pending_recall[n.msg_timestamp] = n.recall_ts;
    lock.unlock();
    if (_chat_model)
        _chat_model->MarkRecalled(n.msg_timestamp, _current_uid);
}

void ChatController::slotOnChatEditNotify(const ChatEditNotifyStruct &n)
{
    if (!_chat_model) return;
    _chat_model->MarkEdited(n.msg_timestamp, n.new_content, n.edit_ts);
}

void ChatController::FlushPendingRecalls()
{
    if (!_chat_model || _pending_recall.isEmpty()) return;
    QMutexLocker lock(&_pending_mutex);
    auto it = _pending_recall.begin();
    while (it != _pending_recall.end()) {
        qint64 ts = it.key();
        ChatMessage dummy;
        if (_chat_model->GetMessageByTimestamp(ts, dummy)) {
            _chat_model->MarkRecalled(ts, _current_uid);
            it = _pending_recall.erase(it);
        } else {
            ++it;
        }
    }
}

// ── 超时清理 ──

void ChatController::slotCleanTimeoutMessages()
{
    QMutexLocker locker(&_pending_mutex);
    const qint64 threshold = QDateTime::currentSecsSinceEpoch() - MESSAGE_TIMEOUT_SEC;
    QStringList expired_ids;

    for (auto it = _pending_messages.cbegin(); it != _pending_messages.cend(); ++it) {
        if (it.value().send_time < threshold)
            expired_ids.append(it.key());
    }

    for (const QString &id : expired_ids) {
        _pending_messages.remove(id);
        DbThreadManager::Instance().UpdateMessageStatus(id, -1);
        if (_chat_model != nullptr)
            _chat_model->UpdateMessageStatus(id, -1);
    }
}

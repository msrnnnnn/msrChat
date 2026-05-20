/**
 * @file ChatController.cpp
 * @brief ChatController 实现
 */
#include "ChatController.h"
#include "FileRecvMgr.h"
#include "FileSendMgr.h"
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

/**
 * @brief 构造函数
 * @details 启动超时清理定时器（每 10 秒检查一次），防止_pending_messages 无限膨胀
 */
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

/**
 * @brief 连接所有业务信号槽
 * @details 包括网络消息、数据库操作、文件传输进度等信号
 */
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
        &DbThreadManager::Instance(), &DbThreadManager::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded,
        Qt::QueuedConnection);
    connect(
        &DbThreadManager::Instance(), &DbThreadManager::sig_messages_saved, this, &ChatController::slotOnMessageSaved,
        Qt::QueuedConnection);
    connect(
        &FileSendMgr::Instance(), &FileSendMgr::sigSendProgress, this,
        [this](int64_t task_id, int progress, int64_t sent, int64_t total)
        { emit sigFileSendProgress(task_id, progress, sent, total); }, Qt::QueuedConnection);
    connect(
        &FileSendMgr::Instance(), &FileSendMgr::sigSendComplete, this,
        [this](int64_t task_id, bool success, const QString &error)
        { emit sigFileSendComplete(task_id, success, error); }, Qt::QueuedConnection);
    connect(
        &FileRecvMgr::Instance(), &FileRecvMgr::SigRecvProgress, this,
        [this](int64_t task_id, int progress, int64_t received, int64_t total)
        { emit sigFileRecvProgress(task_id, progress, received, total); }, Qt::QueuedConnection);
    connect(
        &FileRecvMgr::Instance(), &FileRecvMgr::SigRecvComplete, this,
        [this](int64_t task_id, const QString &filepath, bool success, const QString &error)
        { emit sigFileRecvComplete(task_id, filepath, success, error); }, Qt::QueuedConnection);
}

/**
 * @brief 断开所有业务信号槽
 * @details 析构时调用，防止对象析构后仍有信号触发
 */
void ChatController::DisconnectSignals()
{
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_text_msg, this, &ChatController::slotOnChatTextMsg);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_ack, this, &ChatController::slotOnChatAck);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_con_success, this, &ChatController::slotOnConnectionStateChanged);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_offline_ack, this, &ChatController::slotOnOfflineProgress);
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_reconnected, this, &ChatController::slotOnReconnected);
    disconnect(
        &DbThreadManager::Instance(), &DbThreadManager::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded);
    disconnect(&DbThreadManager::Instance(), &DbThreadManager::sig_messages_saved, this, &ChatController::slotOnMessageSaved);
    disconnect(&FileSendMgr::Instance(), nullptr, this, nullptr);
    disconnect(&FileRecvMgr::Instance(), nullptr, this, nullptr);
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

/**
 * @brief 设置目标聊天用户
 * @param uid 目标用户 ID
 * @details 切换聊天对象时清空当前消息列表并重新加载历史记录
 */
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

/**
 * @brief 发送文本消息
 * @param content 消息内容
 * @details 限制 512 字符，生成客户端消息 ID 后通过 TcpMgr 发送并持久化到数据库
 */
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

    QMutexLocker locker(&_pending_mutex);
    _pending_messages.insert(client_msg_id, PendingMessageInfo{QDateTime::currentSecsSinceEpoch()});
    if (_chat_model != nullptr)
    {
        _chat_model->AddMessage(msg);
    }

    DbThreadManager::Instance().SaveMessage(msg);

    ChatTextReqStruct req;
    req.from_uid = msg.from_uid;
    req.to_uid = msg.to_uid;
    req.content = msg.content;
    req.client_msg_id = msg.client_msg_id;
    TcpMgr::Instance()->slot_send_chat_text_req(req);
}

/**
 * @brief 发送文件
 * @param filePath 文件路径
 * @details 兼容 file:/// URL 格式，生成任务 ID 后分片发送
 */
void ChatController::sendFile(const QString &filePath)
{
    qDebug() << "[ChatController] sendFile called, path:" << filePath << "target_uid:" << _target_uid;

    if (_target_uid <= 0)
    {
        qWarning() << "[ChatController] sendFile failed: target_uid is" << _target_uid;
        emit sigError(QStringLiteral("请先指定目标用户"));
        return;
    }

    // 跨平台处理 URL 前缀 (兼容 QML 传入的 file:/// 或 file:///)
    QString cleanPath = filePath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);  // 删除 "file:///"
    } else if (cleanPath.startsWith("file://")) {
        cleanPath = cleanPath.mid(7);  // 删除 "file://"
    }
    // 处理 Windows 盘符前的多余斜杠，如 /C:/ → C:/
    if (cleanPath.startsWith("/") && cleanPath.length() >= 3 && cleanPath[2] == ':') {
        cleanPath = cleanPath.mid(1);
    }

    qDebug() << "[ChatController] cleanPath after processing:" << cleanPath;

    QFileInfo fileInfo(cleanPath);
    qDebug() << "[ChatController] file exists:" << fileInfo.exists() << "isFile:" << fileInfo.isFile();
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        qWarning() << "[ChatController] sendFile failed: file does not exist or path invalid:" << cleanPath;
        emit sigError(QStringLiteral("文件不存在或路径无效"));
        return;
    }

    // 生成临时任务 ID (毫秒级时间戳)
    int64_t task_id = QDateTime::currentMSecsSinceEpoch();
    int64_t total_size = fileInfo.size();

    qDebug() << "[ChatController] Starting file send, task_id:" << task_id << "size:" << total_size;

    // 1. 发送文件传输握手请求
    TcpMgr::FileReqStruct req;
    req.task_id = task_id;
    req.from_uid = _current_uid;
    req.to_uid = _target_uid;
    req.filename = fileInfo.fileName();
    req.total_size = total_size;
    req.md5 = ""; // 延迟或异步计算 MD5 以防止阻塞主线程
    TcpMgr::Instance()->slot_send_file_req(req);

    // 2. 压入发送队列，等待对端 FileRsp 后自动分片发送
    FileSendMgr::Instance().StartSend(task_id, _target_uid, cleanPath);

    // 3. 通知 QML 进度面板显示条目
    emit sigFileSendStarted(task_id, fileInfo.fileName(), total_size);
}

/**
 * @brief 加载与目标用户的聊天历史记录
 */
void ChatController::loadHistory()
{
    if (_current_uid <= 0 || _target_uid <= 0)
    {
        return;
    }

    DbThreadManager::Instance().GetMessages(_current_uid, _target_uid, LLONG_MAX, HISTORY_PAGE_SIZE);
}

/**
 * @brief 清空聊天历史记录（暂未实现）
 */
void ChatController::clearHistory()
{
}

/**
 * @brief 收到服务器聊天消息处理
 * @param msg 聊天消息结构体
 * @details 持久化到数据库并更新 UI（仅对当前聊天窗口有效）
 */
void ChatController::slotOnChatTextMsg(const ChatTextMsgStruct &msg)
{
    // 直接构造实体，信任底层的幂等性与 Upsert 逻辑
    ChatMessage chat_msg;
    chat_msg.client_msg_id = msg.client_msg_id;
    chat_msg.server_msg_id = msg.server_msg_id;
    chat_msg.from_uid = msg.from_uid;
    chat_msg.to_uid = msg.to_uid;
    chat_msg.content = msg.content;
    chat_msg.timestamp = msg.timestamp;
    chat_msg.status = 1;

    // 1. DB 层持久化
    DbThreadManager::Instance().SaveMessage(chat_msg);

    // 2. UI 层渲染更新
    if (_chat_model != nullptr &&
       ((chat_msg.from_uid == _target_uid && chat_msg.to_uid == _current_uid) ||
        (chat_msg.from_uid == _current_uid && chat_msg.to_uid == _target_uid)))
    {
        _chat_model->UpsertMessage(chat_msg);
    }
}

/**
 * @brief 消息送达/已存储回执处理
 * @param ack 送达回执结构体
 * @details 从 pending 列表移除并更新消息状态（0=发送中，1=已送达，2=已存储）
 */
void ChatController::slotOnChatAck(const ChatAckStruct &ack)
{
    QMutexLocker locker(&_pending_mutex);
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

    DbThreadManager::Instance().UpdateMessageStatus(ack.client_msg_id, status);
    if (_chat_model != nullptr)
    {
        _chat_model->UpdateMessageStatus(ack.client_msg_id, status);
    }

    if (ack.error != 0)
    {
        emit sigError(ack.message.isEmpty() ? QStringLiteral("发送失败") : ack.message);
    }
}

/**
 * @brief 离线消息同步进度处理
 * @param ack 离线 ack 结构体
 * @details 持续发送 ACK 直到接收完所有离线消息
 */
void ChatController::slotOnOfflineProgress(const OfflineAckStruct &ack)
{
    if (ack.received <= _last_offline_received)
    {
        return;
    }
    _last_offline_received = ack.received;

    if (ack.received < ack.total)
    {
        OfflineAckReqStruct req;
        req.received = ack.received;
        TcpMgr::Instance()->slot_send_offline_ack_req(req);
    }
}

/**
 * @brief 连接状态变化处理
 * @param connected 是否已连接
 */
void ChatController::slotOnConnectionStateChanged(bool connected)
{
    if (_is_connected == connected)
    {
        return;
    }

    _is_connected = connected;
    emit sigConnectionStatusChanged();
}

/**
 * @brief 重连后恢复聊天会话
 * @details 重新发送 ChatLoginReq 以维持会话状态
 */
void ChatController::slotOnReconnected()
{
    ChatLoginReqStruct req;
    req.uid = UserMgr::Instance()->GetUid();
    req.token = UserMgr::Instance()->GetToken();
    TcpMgr::Instance()->slot_send_chat_login_req(req);
}

/**
 * @brief 历史消息加载完成回调
 * @param messages 消息列表
 */
void ChatController::slotOnHistoryLoaded(const QVector<ChatMessage> &messages)
{
    if (_chat_model != nullptr)
    {
        _chat_model->SetMessages(messages);
    }
}

/**
 * @brief 消息保存结果回调
 * @param success 是否保存成功
 */
void ChatController::slotOnMessageSaved(bool success)
{
    if (!success)
    {
        emit sigError(QStringLiteral("消息保存失败"));
    }
}

/**
 * @brief 定时清理超时消息（30 秒超时）
 * @details 防止网络异常时 pending 消息无限累积
 */
void ChatController::slotCleanTimeoutMessages()
{
    QMutexLocker locker(&_pending_mutex);
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
        DbThreadManager::Instance().UpdateMessageStatus(id, -1);
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

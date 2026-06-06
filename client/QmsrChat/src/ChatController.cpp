/**
 * @file ChatController.cpp
 * @brief ChatController 实现
 */
#include "ChatController.h"
#include "FileRecvMgr.h"
#include "FileSendMgr.h"
#include "ImageDownloadMgr.h"
#include <QClipboard>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
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

void ChatController::drainBufferedMessages(const QVector<ChatTextMsgStruct> &msgs)
{
    for (const ChatTextMsgStruct &msg : msgs)
    {
        slotOnChatTextMsg(msg);
    }
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
        TcpMgr::Instance(), &TcpMgr::sig_chat_login_rsp, this, &ChatController::slotOnChatLoginRsp,
        Qt::QueuedConnection);
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
        {
            // Phase D — 图片文件接收完成 → 更新 ChatListModel
            if (success && _chat_model)
            {
                QFileInfo fi(filepath);
                QString stem = fi.completeBaseName();
                QUuid uuid(stem);
                if (!uuid.isNull())
                {
                    _chat_model->UpdateImagePath(stem, filepath);
                    DbThreadManager::Instance().UpdateImagePath(stem, filepath);
                    ImageDownloadMgr::Instance().OnFileRecvComplete(stem, filepath, true);
                }
            }
            emit sigFileRecvComplete(task_id, filepath, success, error);
        }, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatImage, this, &ChatController::slotOnChatImage, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigImageDownloadRsp, this, &ChatController::slotOnImageDownloadRsp, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatRecallRsp, this, &ChatController::slotOnChatRecallRsp, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatEditAck, this, &ChatController::slotOnChatEditAck, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatRecallNotify, this, &ChatController::slotOnChatRecallNotify, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigChatEditNotify, this, &ChatController::slotOnChatEditNotify, Qt::QueuedConnection);

    // Phase 6/C — 把 sigSendRecallMsg / sigSendEditMsg 桥接到 TcpMgr 发送
    connect(this, &ChatController::sigSendRecallMsg,
            TcpMgr::Instance(), &TcpMgr::slot_send_chat_recall, Qt::QueuedConnection);
    connect(this, &ChatController::sigSendEditMsg,
            TcpMgr::Instance(), &TcpMgr::slot_send_chat_edit, Qt::QueuedConnection);

    // Phase D — sigSendImageMsg 桥接到 TcpMgr
    connect(this, &ChatController::sigSendImageMsg,
            TcpMgr::Instance(), &TcpMgr::slot_send_chat_image, Qt::QueuedConnection);

    // Phase D — ImageDownloadMgr 请求下载 → TcpMgr 发送协议消息
    connect(&ImageDownloadMgr::Instance(), &ImageDownloadMgr::sigRequestDownload,
            TcpMgr::Instance(), &TcpMgr::slot_send_image_download_req, Qt::QueuedConnection);
    connect(&ImageDownloadMgr::Instance(), &ImageDownloadMgr::sigImageFailed,
            this, [this](const QString &image_id, int reason) {
                if (_chat_model) {
                    _chat_model->UpdateImagePath(image_id, QStringLiteral("error"));
                }
                qWarning() << "[ChatController] image download permanently failed:" << image_id << "reason:" << reason;
            }, Qt::QueuedConnection);
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
    disconnect(TcpMgr::Instance(), &TcpMgr::sig_chat_login_rsp, this, &ChatController::slotOnChatLoginRsp);
    disconnect(
        &DbThreadManager::Instance(), &DbThreadManager::sig_messages_loaded, this, &ChatController::slotOnHistoryLoaded);
    disconnect(&DbThreadManager::Instance(), &DbThreadManager::sig_messages_saved, this, &ChatController::slotOnMessageSaved);
    disconnect(this, &ChatController::sigSendRecallMsg, TcpMgr::Instance(), &TcpMgr::slot_send_chat_recall);
    disconnect(this, &ChatController::sigSendEditMsg,   TcpMgr::Instance(), &TcpMgr::slot_send_chat_edit);
    disconnect(this, &ChatController::sigSendImageMsg,  TcpMgr::Instance(), &TcpMgr::slot_send_chat_image);
    disconnect(&ImageDownloadMgr::Instance(), &ImageDownloadMgr::sigRequestDownload,
               TcpMgr::Instance(), &TcpMgr::slot_send_image_download_req);
    disconnect(&ImageDownloadMgr::Instance(), &ImageDownloadMgr::sigImageFailed, this, nullptr);
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
    _max_received_timestamp = 0;
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
 * @details 限制 4096 字符，生成客户端消息 ID 后通过 TcpMgr 发送并持久化到数据库
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

    if (trimmed.size() > 4096)
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
    msg.timestamp = qMax(QDateTime::currentMSecsSinceEpoch(), _max_received_timestamp + 1);
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
    req.timestamp = msg.timestamp;
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
    FileReqStruct req;
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

void ChatController::sendImage(const QString &imagePath, const QString &caption)
{
    qDebug() << "[ChatController] sendImage called, path:" << imagePath << "caption_len:" << caption.size();

    if (_target_uid <= 0)
    {
        emit sigError(QStringLiteral("请先指定目标用户"));
        return;
    }

    // URL 前缀清理
    QString cleanPath = imagePath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    } else if (cleanPath.startsWith("file://")) {
        cleanPath = cleanPath.mid(7);
    }
    if (cleanPath.startsWith("/") && cleanPath.length() >= 3 && cleanPath[2] == ':') {
        cleanPath = cleanPath.mid(1);
    }

    QFileInfo fileInfo(cleanPath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        emit sigError(QStringLiteral("图片不存在或路径无效"));
        return;
    }

    // 图片元数据
    QImage img(cleanPath);
    if (img.isNull())
    {
        emit sigError(QStringLiteral("无法读取图片"));
        return;
    }
    QString ext = fileInfo.suffix().toLower();
    int64_t total_size = fileInfo.size();

    // 标识符
    QString image_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int64_t task_id = QDateTime::currentMSecsSinceEpoch();
    QString filename = image_id + "." + ext;  // 服务端靠此检测图片模式

    // 1. 发送 FileReq
    FileReqStruct req;
    req.task_id = task_id;
    req.from_uid = _current_uid;
    req.to_uid = _target_uid;
    req.filename = filename;
    req.total_size = total_size;
    req.md5 = "";
    TcpMgr::Instance()->slot_send_file_req(req);

    // 2. 启动文件发送队列
    FileSendMgr::Instance().StartSend(task_id, _target_uid, cleanPath);

    // 3. 构造 ImageMsg 元数据并发送
    ChatImageStruct imgMsg;
    imgMsg.from_uid = _current_uid;
    imgMsg.to_uid = _target_uid;
    imgMsg.image_id = image_id;
    imgMsg.caption = caption;
    imgMsg.timestamp = qMax(QDateTime::currentMSecsSinceEpoch(), _max_received_timestamp + 1);
    imgMsg.width = img.width();
    imgMsg.height = img.height();
    imgMsg.ext = ext;
    imgMsg.size = total_size;
    imgMsg.md5 = "";
    emit sigSendImageMsg(imgMsg);

    // 4. 本地模型预插入
    if (_chat_model)
    {
        ChatMessage m;
        m.from_uid = _current_uid;
        m.to_uid = _target_uid;
        m.type = 1;
        m.image_id = image_id;
        m.image_width = img.width();
        m.image_height = img.height();
        m.image_ext = ext;
        m.content = caption;
        m.timestamp = imgMsg.timestamp;
        m.image_path = cleanPath;  // 发送方本地路径
        m.client_msg_id = image_id;
        _chat_model->AddMessage(m);
    }

    // 5. 通知 QML
    emit sigFileSendStarted(task_id, filename, total_size);
}

void ChatController::slotOnChatImage(const ChatImageStruct &msg)
{
    ChatMessage m;
    m.from_uid = msg.from_uid;
    m.to_uid = msg.to_uid;
    m.type = 1;  // image
    m.image_id = msg.image_id;
    m.image_width = msg.width;
    m.image_height = msg.height;
    m.image_ext = msg.ext;
    m.content = msg.caption;
    m.timestamp = msg.timestamp;
    _max_received_timestamp = qMax(_max_received_timestamp, msg.timestamp);
    m.status = 1;
    m.client_msg_id = msg.image_id;
    // 竞态防护：如果文件已先于 ImageMsg 到达并缓存
    if (ImageDownloadMgr::Instance().IsCached(msg.image_id))
    {
        m.image_path = ImageDownloadMgr::Instance().GetCachePath(msg.image_id, msg.ext);
    }

    // 1. DB 层持久化（始终保存，与 slotOnChatTextMsg 对齐）
    DbThreadManager::Instance().SaveMessage(m);

    // 2. UI 层渲染更新（仅当前目标会话匹配时才显示）
    if (_chat_model != nullptr &&
       ((m.from_uid == _target_uid && m.to_uid == _current_uid) ||
        (m.from_uid == _current_uid && m.to_uid == _target_uid)))
    {
        _chat_model->AddMessage(m);
    }

    // 3. 如果本地没有缓存，自动触发图片下载
    if (m.image_path.isEmpty())
    {
        ImageDownloadMgr::Instance().Request(msg.image_id, 0, msg.ext);
    }
}

void ChatController::slotOnImageDownloadRsp(const ImageDownloadRspStruct &rsp)
{
    if (!_chat_model) return;
    // Phase D — 转发给 ImageDownloadMgr 处理
    ImageDownloadMgr::Instance().OnDownloadRsp(rsp.error, rsp.image_id, rsp.offset);
    if (rsp.error != 0) {
        qWarning() << "[ChatController] image download failed: id=" << rsp.image_id << "error=" << rsp.error;
    }
}

void ChatController::slotOnChatRecallRsp(const ChatEditAckStruct &ack)
{
    if (ack.error != 0) {
        qWarning() << "[ChatController] recall failed: error=" << ack.error;
        emit sigError(QStringLiteral("Recall failed (error %1)").arg(ack.error));
        return;
    }
    // 撤回成功：本地标记消息为已撤回（带 _current_uid 用于 DB 作用域）
    if (_chat_model) {
        _chat_model->MarkRecalled(ack.msg_timestamp, _current_uid);
    }
}

void ChatController::slotOnChatEditAck(const ChatEditAckStruct &ack)
{
    if (ack.error != 0) {
        qWarning() << "[ChatController] edit failed: error=" << ack.error;
        emit sigError(QStringLiteral("Edit failed (error %1)").arg(ack.error));
        return;
    }
    // 编辑成功：本地更新消息内容
    if (_chat_model && !ack.new_content.isEmpty()) {
        _chat_model->MarkEdited(ack.msg_timestamp, ack.new_content, ack.edit_ts);
    }
}

void ChatController::slotOnChatRecallNotify(const ChatRecallNotifyStruct &n)
{
    if (!_chat_model) return;
    _chat_model->MarkRecalled(n.msg_timestamp, _current_uid);
}

void ChatController::slotOnChatEditNotify(const ChatEditNotifyStruct &n)
{
    if (!_chat_model) return;
    _chat_model->MarkEdited(n.msg_timestamp, n.new_content, n.edit_ts);
}

/**
 * @brief 加载与目标用户的聊天历史记录
 */
void ChatController::loadHistory()
{
    _has_more_history = true;
    if (_current_uid <= 0 || _target_uid <= 0)
    {
        return;
    }

    DbThreadManager::Instance().GetMessages(_current_uid, _target_uid, LLONG_MAX, HISTORY_PAGE_SIZE);
}

/**
 * @brief 加载更早的历史消息（向上滚动分页）
 */
void ChatController::loadMoreHistory()
{
    if (_current_uid <= 0 || _target_uid <= 0 || !_has_more_history)
    {
        return;
    }

    qint64 before_time = LLONG_MAX;
    if (_chat_model != nullptr && _chat_model->rowCount() > 0)
    {
        before_time = _chat_model->GetEarliestTimestamp();
    }

    DbThreadManager::Instance().GetMessages(_current_uid, _target_uid, before_time, HISTORY_PAGE_SIZE);
}

/**
 * @brief 清空聊天历史记录
 */
void ChatController::clearHistory()
{
    if (_current_uid <= 0 || _target_uid <= 0)
    {
        return;
    }
    DbThreadManager::Instance().DeleteMessages(_current_uid, _target_uid);
    if (_chat_model != nullptr)
    {
        _chat_model->ClearMessages();
    }
    emit sigHistoryCleared();
}

void ChatController::searchMessages(const QString &keyword)
{
    if (_current_uid <= 0 || _target_uid <= 0 || keyword.trimmed().isEmpty())
    {
        return;
    }

    DbThreadManager::Instance().SearchMessages(_current_uid, _target_uid, keyword.trimmed(), 50);
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
    _max_received_timestamp = qMax(_max_received_timestamp, msg.timestamp);
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

void ChatController::slotOnChatLoginRsp(const ChatLoginRspStruct &rsp)
{
    if (rsp.error != 0)
    {
        qWarning() << "ChatController: chat login re-auth failed, error:" << rsp.error;
        _is_connected = false;
        emit sigConnectionStatusChanged();
        emit sigError(rsp.message.isEmpty() ? QStringLiteral("聊天会话恢复失败") : rsp.message);
        return;
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
        _chat_model->PrependMessages(messages);
    }
    _has_more_history = (messages.size() >= HISTORY_PAGE_SIZE);
    emit sigHasMoreHistoryChanged();

    // 对加载的历史图片消息，检查本地缓存状态
    for (const auto &msg : messages)
    {
        if (msg.type == 1 && msg.image_path.isEmpty() && !msg.image_id.isEmpty())
        {
            if (ImageDownloadMgr::Instance().IsCached(msg.image_id))
            {
                // 已缓存：直接用本地路径更新模型（DB 中 image_path 可能为空）
                QString cachePath = ImageDownloadMgr::Instance().GetCachePath(msg.image_id, msg.image_ext);
                if (_chat_model)
                {
                    _chat_model->UpdateImagePath(msg.image_id, cachePath);
                }
            }
            else
            {
                ImageDownloadMgr::Instance().Request(msg.image_id, 0, msg.image_ext);
            }
        }
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

/**
 * @brief 打开图片查看器（Phase 5）
 * @param imageId 触发查看的图片 UUID
 * @details 扫描当前会话所有 type==1 且未撤回的图片，构造 QVariantList
 *          并发出 sigShowImageViewer 信号。QML 端用 Loader 接收并弹出 ImageViewer。
 *          文本消息和已撤回图片不进入 Viewer 序列（spec section 6.2 v1 决策）。
 */
void ChatController::openImageViewer(const QString &imageId)
{
    QVariantList list;
    int current = 0;
    int idx = 0;

    if (_chat_model != nullptr)
    {
        const auto messages = _chat_model->GetAllMessages();  // 拷贝，安全
        for (const auto &m : messages)
        {
            if (m.type != 1) continue;  // 1=image
            if (m.recalled) continue;   // 撤回图片不进 Viewer
            if (m.image_id == imageId) current = idx;

            QVariantMap entry;
            entry["imageId"] = m.image_id;
            entry["imagePath"] = m.image_path;
            entry["caption"] = m.content;  // caption 复用 content 字段（P3 决策）
            list.append(entry);
            ++idx;
        }
    }

    emit sigShowImageViewer(list, current);
}

/**
 * @brief 获取当前会话所有图片列表（供 ImageViewer 初始化 / 刷新用）
 * @return QVariantList，每项 {imageId, imagePath, caption}
 */
QVariantList ChatController::getImageListForViewer() const
{
    QVariantList list;

    if (_chat_model != nullptr)
    {
        const auto messages = _chat_model->GetAllMessages();
        for (const auto &m : messages)
        {
            if (m.type != 1) continue;
            if (m.recalled) continue;

            QVariantMap entry;
            entry["imageId"] = m.image_id;
            entry["imagePath"] = m.image_path;
            entry["caption"] = m.content;
            list.append(entry);
        }
    }

    return list;
}

// === Phase 6 — 右键菜单 6 项 action 实现 ===

/**
 * @brief 菜单项：回复（v1 stub）
 * @details 在输入框插入"回复 {from_uid}: "前缀；QML 端 onSigSetReplyContext 处理实际插入
 */
void ChatController::actionReply(qint64 timestamp)
{
    if (_chat_model == nullptr) return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp)
        {
            const QString prefix = QStringLiteral("回复 %1: ").arg(m.from_uid);
            emit sigSetReplyContext(prefix);
            return;
        }
    }
}

/**
 * @brief 菜单项：复制文字（v1 走系统剪贴板）
 * @details QGuiApplication::clipboard() 跨平台，Windows / Linux / macOS 都可用
 */
void ChatController::actionCopyText(qint64 timestamp)
{
    if (_chat_model == nullptr) return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp)
        {
            QGuiApplication::clipboard()->setText(m.content);
            return;
        }
    }
}

/**
 * @brief 菜单项：撤回（仅自方 + 2 分钟内）
 * @details 构造 ChatRecallMsgStruct → emit sigSendRecallMsg → TcpMgr::slot_send_chat_recall
 *          实际服务端校验 + DB 标记 + Notify 推送由 P7 补全
 */
void ChatController::actionRecall(qint64 timestamp)
{
    if (_chat_model == nullptr) return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp && m.from_uid == _current_uid)
        {
            ChatRecallMsgStruct req;
            req.from_uid = _current_uid;
            req.msg_timestamp = timestamp;
            req.client_msg_id = m.client_msg_id;
            emit sigSendRecallMsg(req);
            return;
        }
    }
}

/**
 * @brief 菜单项：编辑（仅自方，2 分钟校验在 P7 服务端做）
 * @details 长度校验本地做（避免显然非法的请求发到服务端），其他校验走服务端
 */
void ChatController::actionEdit(qint64 timestamp, const QString &newContent)
{
    if (newContent.isEmpty() || newContent.size() > 4096) return;
    if (_chat_model == nullptr) return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp && m.from_uid == _current_uid)
        {
            ChatEditMsgStruct req;
            req.from_uid = _current_uid;
            req.msg_timestamp = timestamp;
            req.new_content = newContent;
            emit sigSendEditMsg(req);  // P3 已有的 signal
            return;
        }
    }
}

/**
 * @brief 菜单项：另存为（仅图片，v1 stub 只 emit signal）
 * @details QML 端 onSigShowSaveAsDialog 接住，弹 FileDialog 选目标路径
 *          实际 copy 逻辑 v1 暂省略，事件 P8 / v2 补
 */
void ChatController::actionSaveAs(qint64 timestamp)
{
    if (_chat_model == nullptr) return;
    for (const auto &m : _chat_model->GetAllMessages())
    {
        if (m.timestamp == timestamp && m.type == 1)
        {
            emit sigShowSaveAsDialog(m.image_path);
            return;
        }
    }
}

/**
 * @brief 菜单项：删除（仅本地，不通知对端）
 * @details model 移除 + DB 单条删除。**修复 plan bug** — 不用 DeleteMessages(uid1, uid2)
 */
void ChatController::actionDelete(qint64 timestamp)
{
    if (_chat_model == nullptr) return;
    _chat_model->RemoveMessageByTimestamp(timestamp);
    DbThreadManager::Instance().DeleteMessageByTimestamp(timestamp);
}

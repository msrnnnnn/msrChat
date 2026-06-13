/**
 * @file tcpmgr.cpp
 * @brief TCP 管理单例实现
 */
#include "TcpMgr.h"
#include "FileRecvMgr.h"
#include "FileSendMgr.h"
#include "Message.pb.h"
#include "TcpWorker.h"
#include "Utils.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>
#include <mutex>

namespace {
/**
 * @brief 将 JSON 对象转换为紧凑格式的 QByteArray
 * @param obj JSON 对象
 * @return QByteArray 紧凑 JSON 字节数组
 */
QByteArray MakeJsonPayload(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}
} // namespace

TcpMgr *TcpMgr::_instance = nullptr;

/**
 * @brief 获取 TcpMgr 单例实例
 * @return TcpMgr* 单例指针
 * @note 使用 std::call_once 保证多线程环境下的单例安全
 */
TcpMgr *TcpMgr::Instance()
{
    static std::once_flag flag;
    std::call_once(flag, []{ _instance = new TcpMgr(); });
    return _instance;
}

/**
 * @brief 初始化单例（无操作，仅触发构造）
 */
void TcpMgr::Init()
{
    Instance();
}

/**
 * @brief 销毁单例，释放资源
 */
void TcpMgr::Destroy()
{
    delete _instance;
    _instance = nullptr;
}

/**
 * @brief 构造函数
 * @param parent 父对象
 * @note 注册元类型是为了跨线程信号槽传递自定义类型
 */
TcpMgr::TcpMgr(QObject *parent)
    : QObject(parent),
      _netThread(new QThread(this)),
      _worker(new TcpWorker())
{
    qRegisterMetaType<RequestType>("RequestType");
    qRegisterMetaType<ServerInfo>("ServerInfo");
    qRegisterMetaType<LoginReqStruct>("LoginReqStruct");
    qRegisterMetaType<ChatLoginReqStruct>("ChatLoginReqStruct");
    qRegisterMetaType<ChatTextReqStruct>("ChatTextReqStruct");
    qRegisterMetaType<VerifyCodeReqStruct>("VerifyCodeReqStruct");
    qRegisterMetaType<VerifyCodeRspStruct>("VerifyCodeRspStruct");
    qRegisterMetaType<RegisterReqStruct>("RegisterReqStruct");
    qRegisterMetaType<RegisterRspStruct>("RegisterRspStruct");
    qRegisterMetaType<ResetPwdReqStruct>("ResetPwdReqStruct");
    qRegisterMetaType<ResetPwdRspStruct>("ResetPwdRspStruct");
    qRegisterMetaType<OfflineAckReqStruct>("OfflineAckReqStruct");
    qRegisterMetaType<ChatTextMsgStruct>("ChatTextMsgStruct");
    qRegisterMetaType<ChatAckStruct>("ChatAckStruct");
    qRegisterMetaType<OfflineAckStruct>("OfflineAckStruct");
    qRegisterMetaType<LoginRspStruct>("LoginRspStruct");
    qRegisterMetaType<ChatLoginRspStruct>("ChatLoginRspStruct");
    qRegisterMetaType<ChatImageStruct>("ChatImageStruct");
    qRegisterMetaType<ImageDownloadRspStruct>("ImageDownloadRspStruct");
    qRegisterMetaType<ChatRecallMsgStruct>("ChatRecallMsgStruct");
    qRegisterMetaType<ChatRecallNotifyStruct>("ChatRecallNotifyStruct");
    qRegisterMetaType<ChatEditMsgStruct>("ChatEditMsgStruct");
    qRegisterMetaType<ChatEditAckStruct>("ChatEditAckStruct");
    qRegisterMetaType<ChatEditNotifyStruct>("ChatEditNotifyStruct");
    init_thread();
}

/**
 * @brief 析构函数
 */
TcpMgr::~TcpMgr()
{
    if (_worker)
    {
        emit sigStopWorker();
        // 将 delete 操作投递到工作线程的事件循环，确保在工作线程内析构 QTcpSocket 等对象
        _worker->deleteLater();
    }
    if (_netThread)
    {
        _netThread->quit();
        if (!_netThread->wait(5000))
        {
            qWarning() << "TcpMgr: netThread did not finish in 5s";
            _netThread->terminate();
            _netThread->wait();
        }
    }
    // 避免悬空指针
    _worker = nullptr;
}

/**
 * @brief 获取当前 TCP 连接状态
 * @return true 已连接, false 未连接
 */
bool TcpMgr::IsConnected() const
{
    return _is_connected;
}

/**
 * @brief 初始化网络线程与信号连接
 */
void TcpMgr::init_thread()
{
    _worker->moveToThread(_netThread);
    connect(_netThread, &QThread::started, _worker, &TcpWorker::slot_init);
    // 删除 finished->deleteLater：会导致 deleteLater 投递到已退出 worker 线程的事件队列

    connect(_worker, &TcpWorker::sigConSuccess, this,
            [this](bool connected) {
                _is_connected = connected;
                emit sigConSuccess(connected);
            },
            Qt::QueuedConnection);
    connect(_worker, &TcpWorker::sigReconnected, this, &TcpMgr::sigReconnected, Qt::QueuedConnection);

    connect(_worker, &TcpWorker::sigPacketReceived, this, &TcpMgr::slotDispatchPacket, Qt::QueuedConnection);

    connect(this, &TcpMgr::sigStopWorker, _worker, &TcpWorker::slot_stop, Qt::QueuedConnection);
    connect(this, &TcpMgr::sigConnectWorker, _worker, &TcpWorker::slotTcpConnect, Qt::QueuedConnection);
    connect(this, &TcpMgr::sigSendDataWorker, _worker, &TcpWorker::slotSendData, Qt::QueuedConnection);

    _netThread->start();
}

/**
 * @brief 发送连接请求到工作线程
 * @param si 服务器连接信息
 */
void TcpMgr::slotTcpConnect(ServerInfo si)
{
    if (!_worker)
    {
        return;
    }
    emit sigConnectWorker(si);
}

/**
 * @brief 发送数据到工作线程
 * @param reqId 请求类型
 * @param data 数据内容
 */
void TcpMgr::slotSendData(RequestType reqId, const QByteArray &data)
{
    if (!_worker)
    {
        return;
    }
    emit sigSendDataWorker(reqId, data);
}

/**
 * @brief 发送登录请求（JSON 格式）
 * @param req 登录请求结构体（用户名、密码）
 */
void TcpMgr::slot_send_login_req(const LoginReqStruct &req)
{
    slotSendData(RequestType::ID_LOGIN_USER, MakeJsonPayload({{"user", req.user}, {"passwd", req.passwd}}));
}

/**
 * @brief 发送聊天服务登录请求（JSON 格式）
 * @param req 聊天登录请求结构体（uid、token）
 */
void TcpMgr::slot_send_chat_login_req(const ChatLoginReqStruct &req)
{
    slotSendData(RequestType::MSG_CHAT_LOGIN, MakeJsonPayload({{"uid", req.uid}, {"token", req.token}}));
}

/**
 * @brief 发送聊天文本消息（Protobuf 序列化）
 * @param req 聊天文本请求结构体
 */
void TcpMgr::slot_send_chat_text_req(const ChatTextReqStruct &req)
{
    // 构建 Protobuf 消息并序列化
    qmsrchat::ChatTextMsg chatMsg;
    chatMsg.set_from_uid(req.from_uid);
    chatMsg.set_to_uid(req.to_uid);
    chatMsg.set_content(req.content.toStdString());
    chatMsg.set_client_msg_id(req.client_msg_id.toStdString());
    chatMsg.set_timestamp(req.timestamp);

    // Phase 5E: 填充防重放 Nonce
    auto *nh = chatMsg.mutable_nonce_header();
    std::string nonce = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    int64_t ts = QDateTime::currentMSecsSinceEpoch();
    nh->set_nonce(nonce);
    nh->set_timestamp(ts);
    nh->set_signature(Utils::hmacSha256(QStringLiteral("MsrChat_v1_Salt_2024"), QString::fromStdString(nonce + std::to_string(ts))).toStdString());

    std::string serialized;
    if (chatMsg.SerializeToString(&serialized))
    {
        slotSendData(RequestType::MSG_CHAT_TEXT, QByteArray(serialized.data(), serialized.size()));
    }
}

/**
 * @brief 发送验证码请求
 * @param req 验证码请求结构体（邮箱）
 */
void TcpMgr::slot_send_verify_code_req(const VerifyCodeReqStruct &req)
{
    slotSendData(RequestType::ID_GET_VERIFY_CODE, MakeJsonPayload({{"email", req.email}}));
}

/**
 * @brief 发送注册请求
 * @param req 注册请求结构体
 */
void TcpMgr::slot_send_register_req(const RegisterReqStruct &req)
{
    slotSendData(RequestType::ID_REGISTER_USER,
                   MakeJsonPayload({{"user", req.user}, {"email", req.email}, {"passwd", req.passwd}, {"verifycode", req.verifycode},
                                   {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                   {"timestamp", QDateTime::currentMSecsSinceEpoch()}}));
}

/**
 * @brief 发送重置密码请求
 * @param req 重置密码请求结构体
 */
void TcpMgr::slot_send_reset_pwd_req(const ResetPwdReqStruct &req)
{
    slotSendData(RequestType::ID_RESET_PWD,
                   MakeJsonPayload({{"user", req.user}, {"email", req.email}, {"passwd", req.passwd}, {"verifycode", req.verifycode},
                                   {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                   {"timestamp", QDateTime::currentMSecsSinceEpoch()}}));
}

/**
 * @brief 发送离线消息确认请求
 * @param req 离线确认请求结构体
 */
void TcpMgr::slot_send_offline_ack_req(const OfflineAckReqStruct &req)
{
    slotSendData(RequestType::MSG_OFFLINE_ACK, MakeJsonPayload({{"received", req.received}}));
}

/**
 * @brief 发送文件传输请求（Protobuf 序列化）
 * @param req 文件请求结构体（task_id、文件名、MD5 等）
 */
void TcpMgr::slot_send_file_req(const FileReqStruct &req)
{
    qDebug() << "[TcpMgr] slot_send_file_req called, task_id:" << req.task_id << "from:" << req.from_uid << "to:" << req.to_uid << "filename:" << req.filename;
    qmsrchat::FileReq fileReq;
    fileReq.set_task_id(req.task_id);
    fileReq.set_from_uid(req.from_uid);
    fileReq.set_to_uid(req.to_uid);
    fileReq.set_filename(req.filename.toStdString());
    fileReq.set_total_size(req.total_size);
    fileReq.set_md5(req.md5.toStdString());
    fileReq.set_offset(req.offset);

    std::string serialized;
    if (fileReq.SerializeToString(&serialized))
    {
        slotSendData(RequestType::MSG_FILE_REQ, QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    }
}

// === Phase 6 — 撤回 / 编辑发送（套用 slot_send_chat_text_req 模板）===

/**
 * @brief 发送撤回消息请求（Protobuf 序列化）
 * @param req 撤回消息结构体
 */
void TcpMgr::slot_send_chat_recall(const ChatRecallMsgStruct &req)
{
    qmsrchat::RecallMsg msg;
    msg.set_from_uid(req.from_uid);
    msg.set_msg_timestamp(req.msg_timestamp);
    msg.set_client_msg_id(req.client_msg_id.toStdString());

    // Phase 5E: 填充防重放 Nonce
    auto *nh = msg.mutable_nonce_header();
    std::string recallNonce = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    int64_t recallTs = QDateTime::currentMSecsSinceEpoch();
    nh->set_nonce(recallNonce);
    nh->set_timestamp(recallTs);
    nh->set_signature(Utils::hmacSha256(QStringLiteral("MsrChat_v1_Salt_2024"), QString::fromStdString(recallNonce + std::to_string(recallTs))).toStdString());

    std::string serialized;
    if (msg.SerializeToString(&serialized))
    {
        slotSendData(RequestType::MSG_CHAT_RECALL, QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    }
}

/**
 * @brief 发送编辑消息请求（Protobuf 序列化）
 * @param req 编辑消息结构体
 */
void TcpMgr::slot_send_chat_edit(const ChatEditMsgStruct &req)
{
    qmsrchat::EditMsg msg;
    msg.set_from_uid(req.from_uid);
    msg.set_msg_timestamp(req.msg_timestamp);
    msg.set_new_content(req.new_content.toStdString());

    // Phase 5E: 填充防重放 Nonce
    auto *nh = msg.mutable_nonce_header();
    std::string editNonce = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    int64_t editTs = QDateTime::currentMSecsSinceEpoch();
    nh->set_nonce(editNonce);
    nh->set_timestamp(editTs);
    nh->set_signature(Utils::hmacSha256(QStringLiteral("MsrChat_v1_Salt_2024"), QString::fromStdString(editNonce + std::to_string(editTs))).toStdString());

    std::string serialized;
    if (msg.SerializeToString(&serialized))
    {
        slotSendData(RequestType::MSG_CHAT_EDIT, QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    }
}

/**
 * @brief 发送图片消息（Protobuf 序列化，含宽高、MD5 等元数据）
 * @param msg 图片消息结构体
 */
void TcpMgr::slot_send_chat_image(const ChatImageStruct &msg)
{
    qmsrchat::ImageMsg imgMsg;
    imgMsg.set_from_uid(msg.from_uid);
    imgMsg.set_to_uid(msg.to_uid);
    imgMsg.set_image_id(msg.image_id.toStdString());
    imgMsg.set_caption(msg.caption.toStdString());
    imgMsg.set_timestamp(msg.timestamp);
    imgMsg.set_width(msg.width);
    imgMsg.set_height(msg.height);
    imgMsg.set_ext(msg.ext.toStdString());
    imgMsg.set_size(msg.size);
    imgMsg.set_md5(msg.md5.toStdString());

    std::string serialized;
    if (imgMsg.SerializeToString(&serialized))
    {
        slotSendData(RequestType::MSG_CHAT_IMAGE,
                       QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    }
}

/**
 * @brief 发送图片下载请求
 * @param image_id 图片唯一 ID
 */
void TcpMgr::slot_send_image_download_req(const QString &image_id)
{
    qmsrchat::ImageDownloadReq req;
    req.set_image_id(image_id.toStdString());

    std::string serialized;
    if (req.SerializeToString(&serialized))
    {
        slotSendData(RequestType::MSG_IMAGE_DOWNLOAD_REQ,
                       QByteArray(serialized.data(), static_cast<int>(serialized.size())));
    }
}

/**
 * @brief 根据消息类型分发数据包到对应的解析器
 * @param msg_id 消息 ID
 * @param data 原始数据
 * @details 登录相关走 JSON 解析器，聊天消息走 Protobuf 解析器，文件传输走专用文件处理流程。
 */
void TcpMgr::slotDispatchPacket(quint16 msg_id, const QByteArray &data)
{
    const auto req_type = static_cast<RequestType>(msg_id);
    switch (req_type)
    {
        case RequestType::ID_LOGIN_USER:
        case RequestType::ID_GET_VERIFY_CODE:
        case RequestType::ID_REGISTER_USER:
        case RequestType::ID_RESET_PWD:
            _parser.parseLoginPacket(req_type, data);
            return;
        case RequestType::MSG_CHAT_LOGIN:
        case RequestType::MSG_CHAT_TEXT:
        case RequestType::MSG_CHAT_ACK:
        case RequestType::MSG_OFFLINE_ACK:
        case RequestType::MSG_CHAT_IMAGE:
        case RequestType::MSG_IMAGE_DOWNLOAD_RSP:
        case RequestType::MSG_CHAT_RECALL:
        case RequestType::MSG_CHAT_EDIT:
        case RequestType::MSG_CHAT_RECALL_NOTIFY:
        case RequestType::MSG_CHAT_EDIT_NOTIFY:
            _parser.parseChatPacket(req_type, data);
            return;
        case RequestType::MSG_FILE_REQ:
        case RequestType::MSG_FILE_RSP:
        case RequestType::MSG_FILE_CHUNK:
        case RequestType::MSG_FILE_ACK:
            handle_file_packet(req_type, data);
            return;
        default:
            return;
    }
}

/**
 * @brief 处理文件传输相关的数据包
 * @param req_type 请求类型
 * @param data 数据内容
 * @details 分发逻辑：MSG_FILE_CHUNK（接收分片并回复 ACK）、MSG_FILE_REQ（请求确认并回复 RSP）、
 *          MSG_FILE_RSP（通知发送方可以传输）、MSG_FILE_ACK（确认已接收分片或传输完成）。
 */
void TcpMgr::handle_file_packet(RequestType req_type, const QByteArray &data)
{
    // MSG_FILE_CHUNK：接收文件分片，写入磁盘后回复 ACK 确认
    if (req_type == RequestType::MSG_FILE_CHUNK)
    {
        qmsrchat::FileChunk chunk;
        if (!chunk.ParseFromArray(data.constData(), data.size()))
        {
            return;
        }

        const int64_t task_id = chunk.task_id();
        const int64_t offset = chunk.offset();
        const QByteArray chunk_data(chunk.data().data(), static_cast<int>(chunk.data().size()));

        int64_t committed = offset;
        QString error;
        const bool ok = !chunk_data.isEmpty() &&
                        FileRecvMgr::Instance().WriteChunk(task_id, offset, chunk_data, &committed, &error);

        qmsrchat::FileAck ack;
        ack.set_task_id(task_id);
        ack.set_error(ok ? 0 : 1);
        ack.set_received(ok ? committed : offset);
        if (!error.isEmpty())
        {
            ack.set_message(error.toStdString());
        }

        std::string serialized;
        if (ack.SerializeToString(&serialized))
        {
            slotSendData(RequestType::MSG_FILE_ACK, QByteArray(serialized.data(), static_cast<int>(serialized.size())));
        }
        return;
    }

    if (req_type == RequestType::MSG_FILE_REQ)
    {
        qmsrchat::FileReq fileReq;
        if (!fileReq.ParseFromArray(data.constData(), data.size()))
        {
            return;
        }

        QString error;
        const bool ok = FileRecvMgr::Instance().StartRecv(
            fileReq.task_id(), fileReq.from_uid(), fileReq.filename(), fileReq.total_size(), fileReq.md5(), &error);

        qmsrchat::FileRsp rsp;
        rsp.set_task_id(fileReq.task_id());
        rsp.set_error(ok ? 0 : 1);
        if (ok)
        {
            rsp.set_offset(FileRecvMgr::Instance().GetReceivedSize(fileReq.task_id()));
        }
        else
        {
            rsp.set_offset(0);
        }
        rsp.set_message((ok ? QStringLiteral("ready to receive") : error).toStdString());

        std::string serialized;
        if (rsp.SerializeToString(&serialized))
        {
            slotSendData(RequestType::MSG_FILE_RSP, QByteArray(serialized.data(), static_cast<int>(serialized.size())));
        }
        return;
    }

    if (req_type == RequestType::MSG_FILE_RSP)
    {
        qmsrchat::FileRsp fileRsp;
        if (!fileRsp.ParseFromArray(data.constData(), data.size()))
        {
            return;
        }

        if (fileRsp.error() == 0)
        {
            FileSendMgr::Instance().OnRecvReady(fileRsp.task_id(), fileRsp.offset());
        }
        else
        {
            FileSendMgr::Instance().CancelSend(fileRsp.task_id());
            qWarning() << "File send rejected by receiver, task_id:" << fileRsp.task_id()
                       << "error:" << QString::fromStdString(fileRsp.message());
        }
        return;
    }

    if (req_type == RequestType::MSG_FILE_ACK)
    {
        qmsrchat::FileAck fileAck;
        if (!fileAck.ParseFromArray(data.constData(), data.size()))
        {
            return;
        }

        if (fileAck.message() == "transfer complete")
        {
            FileSendMgr::Instance().CancelSend(fileAck.task_id());
            return;
        }
        if (fileAck.received() > 0)
        {
            FileSendMgr::Instance().OnRecvReady(fileAck.task_id(), fileAck.received());
        }
    }
}

/**
 * @file tcpmgr.cpp
 * @brief TCP 管理单例实现
 */
#include "TcpMgr.h"
#include "Message.pb.h"
#include "TcpWorker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutexLocker>

QMutex TcpMgr::_mutex;
TcpMgr *TcpMgr::_instance = nullptr;

TcpMgr *TcpMgr::Instance()
{
    if (_instance)
    {
        return _instance;
    }
    QMutexLocker locker(&_mutex);
    if (!_instance)
    {
        _instance = new TcpMgr();
    }
    return _instance;
}

void TcpMgr::Init()
{
    Instance();
}

void TcpMgr::Destroy()
{
    QMutexLocker locker(&_mutex);
    if (_instance)
    {
        delete _instance;
        _instance = nullptr;
    }
}

/**
 * @brief 构造函数
 * @param parent 父对象
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
    init_thread();
}

/**
 * @brief 析构函数
 */
TcpMgr::~TcpMgr()
{
    if (_worker)
    {
        QMetaObject::invokeMethod(_worker, "slot_stop", Qt::QueuedConnection);
    }
    if (_netThread)
    {
        _netThread->quit();
        _netThread->wait();

        if (_worker)
        {
            delete _worker;
            _worker = nullptr;
        }
    }
}

/**
 * @brief 初始化网络线程与信号连接
 */
void TcpMgr::init_thread()
{
    _worker->moveToThread(_netThread);
    connect(_netThread, &QThread::started, _worker, &TcpWorker::slot_init);
    connect(_netThread, &QThread::finished, _worker, &QObject::deleteLater);

    connect(_worker, &TcpWorker::sig_con_success, this, &TcpMgr::sig_con_success, Qt::QueuedConnection);
    connect(_worker, &TcpWorker::sig_reconnected, this, &TcpMgr::sig_reconnected, Qt::QueuedConnection);

    connect(
        _worker, static_cast<void (TcpWorker::*)(RequestType, QByteArray)>(&TcpWorker::sig_msg_received), this,
        &TcpMgr::slot_parse_login_rsp, Qt::QueuedConnection);
    connect(
        _worker, static_cast<void (TcpWorker::*)(quint16, QByteArray)>(&TcpWorker::sig_msg_received), this,
        &TcpMgr::slot_parse_chat_msg, Qt::QueuedConnection);

    _netThread->start();
}

/**
 * @brief 发送连接请求到工作线程
 * @param si 服务器连接信息
 */
void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    if (!_worker)
    {
        return;
    }
    QMetaObject::invokeMethod(_worker, "slot_tcp_connect", Qt::QueuedConnection, Q_ARG(ServerInfo, si));
}

/**
 * @brief 发送数据到工作线程
 * @param reqId 请求类型
 * @param data 数据内容
 */
void TcpMgr::slot_send_data(RequestType reqId, const QByteArray &data)
{
    if (!_worker)
    {
        return;
    }
    QMetaObject::invokeMethod(
        _worker, "slot_send_data", Qt::QueuedConnection, Q_ARG(RequestType, reqId), Q_ARG(QByteArray, data));
}

void TcpMgr::slot_send_login_req(const LoginReqStruct &req)
{
    QJsonObject jsonObj;
    jsonObj["user"] = req.user;
    jsonObj["passwd"] = req.passwd;
    QJsonDocument doc(jsonObj);
    slot_send_data(RequestType::ID_LOGIN_USER, doc.toJson(QJsonDocument::Compact));
}

void TcpMgr::slot_send_chat_login_req(const ChatLoginReqStruct &req)
{
    QJsonObject jsonObj;
    jsonObj["uid"] = req.uid;
    jsonObj["token"] = req.token;
    QJsonDocument doc(jsonObj);
    slot_send_data(RequestType::MSG_CHAT_LOGIN, doc.toJson(QJsonDocument::Compact));
}

void TcpMgr::slot_send_chat_text_req(const ChatTextReqStruct &req)
{
    qmsrchat::ChatTextMsg chatMsg;
    chatMsg.set_from_uid(req.from_uid);
    chatMsg.set_to_uid(req.to_uid);
    chatMsg.set_content(req.content.toStdString());
    chatMsg.set_client_msg_id(req.client_msg_id.toStdString());

    std::string serialized;
    if (chatMsg.SerializeToString(&serialized))
    {
        slot_send_data(RequestType::MSG_CHAT_TEXT, QByteArray(serialized.data(), serialized.size()));
    }
}

void TcpMgr::slot_send_verify_code_req(const VerifyCodeReqStruct &req)
{
    QJsonObject jsonObj;
    jsonObj["email"] = req.email;
    QJsonDocument doc(jsonObj);
    slot_send_data(RequestType::ID_GET_VARIFY_CODE, doc.toJson(QJsonDocument::Compact));
}

void TcpMgr::slot_send_register_req(const RegisterReqStruct &req)
{
    QJsonObject jsonObj;
    jsonObj["user"] = req.user;
    jsonObj["email"] = req.email;
    jsonObj["passwd"] = req.passwd;
    jsonObj["varifycode"] = req.varifycode;
    QJsonDocument doc(jsonObj);
    slot_send_data(RequestType::ID_REGISTER_USER, doc.toJson(QJsonDocument::Compact));
}

void TcpMgr::slot_send_reset_pwd_req(const ResetPwdReqStruct &req)
{
    QJsonObject jsonObj;
    jsonObj["user"] = req.user;
    jsonObj["email"] = req.email;
    jsonObj["passwd"] = req.passwd;
    jsonObj["varifycode"] = req.varifycode;
    QJsonDocument doc(jsonObj);
    slot_send_data(RequestType::ID_RESET_PWD, doc.toJson(QJsonDocument::Compact));
}

void TcpMgr::slot_send_offline_ack_req(const OfflineAckReqStruct &req)
{
    QJsonObject jsonObj;
    jsonObj["received"] = req.received;
    QJsonDocument doc(jsonObj);
    slot_send_data(RequestType::MSG_OFFLINE_ACK, doc.toJson(QJsonDocument::Compact));
}

void TcpMgr::slot_parse_login_rsp(RequestType req_type, const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        return;
    }

    QJsonObject jsonObj = doc.object();

    if (req_type == RequestType::ID_LOGIN_USER)
    {
        LoginRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.uid = jsonObj["uid"].toInt();
        rsp.token = jsonObj["token"].toString();
        rsp.user = jsonObj["user"].toString();

        emit sig_login_rsp(rsp);
    }
    else if (req_type == RequestType::ID_GET_VARIFY_CODE)
    {
        VerifyCodeRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.email = jsonObj["email"].toString();

        emit sig_verify_code_rsp(rsp);
    }
    else if (req_type == RequestType::ID_REGISTER_USER)
    {
        RegisterRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.email = jsonObj["email"].toString();

        emit sig_register_rsp(rsp);
    }
    else if (req_type == RequestType::ID_RESET_PWD)
    {
        ResetPwdRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();

        emit sig_reset_pwd_rsp(rsp);
    }
}

void TcpMgr::slot_parse_chat_login_rsp(quint16 msg_id, const QByteArray &data)
{
    if (msg_id != static_cast<quint16>(RequestType::MSG_CHAT_LOGIN))
    {
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        return;
    }

    QJsonObject obj = doc.object();
    ChatLoginRspStruct rsp;
    rsp.error = obj.value("error").toInt(1);
    rsp.message = obj.value("message").toString();

    emit sig_chat_login_rsp(rsp);
}

void TcpMgr::slot_parse_chat_msg(quint16 msg_id, const QByteArray &data)
{
    if (msg_id == static_cast<quint16>(RequestType::MSG_CHAT_TEXT))
    {
        qmsrchat::ServerChatMsg chatMsg;
        if (chatMsg.ParseFromArray(data.constData(), data.size()))
        {
            ChatTextMsgStruct msg;
            msg.from_uid = chatMsg.from_uid();
            msg.to_uid = chatMsg.to_uid();
            msg.content = QString::fromStdString(chatMsg.content());
            msg.client_msg_id = QString::fromStdString(chatMsg.client_msg_id());
            msg.server_msg_id = chatMsg.server_msg_id();
            msg.timestamp = chatMsg.timestamp();

            emit sig_chat_text_msg(msg);
        }
        else
        {
            qWarning() << "Failed to parse ServerChatMsg from Protobuf";
        }
    }
    else if (msg_id == static_cast<quint16>(RequestType::MSG_CHAT_ACK))
    {
        qmsrchat::ChatAck chatAck;
        if (!chatAck.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse ChatAck from Protobuf";
            return;
        }

        ChatAckStruct ack;
        ack.error = chatAck.error();
        ack.message = QString::fromStdString(chatAck.message());
        ack.client_msg_id = QString::fromStdString(chatAck.client_msg_id());

        emit sig_chat_ack(ack);
    }
    else if (msg_id == static_cast<quint16>(RequestType::MSG_OFFLINE_ACK))
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
        {
            return;
        }

        QJsonObject obj = doc.object();
        OfflineAckStruct ack;
        ack.received = obj.value("received").toInteger();
        ack.total = obj.value("total").toInteger();

        emit sig_offline_ack(ack);
    }
    else if (msg_id == static_cast<quint16>(RequestType::MSG_CHAT_LOGIN))
    {
        slot_parse_chat_login_rsp(msg_id, data);
    }
}

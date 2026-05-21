#include "TcpProtocolParser.h"
#include "Message.pb.h"
#include "ProtocolStructs.h"
#include "TcpMgr.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

void TcpProtocolParser::parseLoginPacket(RequestType req_type, const QByteArray &data)
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

        emit _tcpMgr.sig_login_rsp(rsp);
    }
    else if (req_type == RequestType::ID_GET_VARIFY_CODE)
    {
        VerifyCodeRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.email = jsonObj["email"].toString();
        rsp.code = jsonObj["code"].toInt();

        emit _tcpMgr.sig_verify_code_rsp(rsp);
    }
    else if (req_type == RequestType::ID_REGISTER_USER)
    {
        RegisterRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.email = jsonObj["email"].toString();

        emit _tcpMgr.sig_register_rsp(rsp);
    }
    else if (req_type == RequestType::ID_RESET_PWD)
    {
        ResetPwdRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();

        emit _tcpMgr.sig_reset_pwd_rsp(rsp);
    }
}

void TcpProtocolParser::parseChatPacket(RequestType req_type, const QByteArray &data)
{
    if (req_type == RequestType::MSG_CHAT_LOGIN)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
        {
            return;
        }

        QJsonObject obj = doc.object();
        ChatLoginRspStruct rsp;
        rsp.error = obj.value("error").toInt(1);
        rsp.message = obj.value("message").toString();

        emit _tcpMgr.sig_chat_login_rsp(rsp);
        return;
    }

    if (req_type == RequestType::MSG_CHAT_TEXT)
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

            emit _tcpMgr.sig_chat_text_msg(msg);
        }
        else
        {
            qWarning() << "Failed to parse ServerChatMsg from Protobuf";
        }
        return;
    }
    if (req_type == RequestType::MSG_CHAT_ACK)
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

        emit _tcpMgr.sig_chat_ack(ack);
        return;
    }
    if (req_type == RequestType::MSG_OFFLINE_ACK)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
        {
            return;
        }

        QJsonObject jsonObj = doc.object();
        OfflineAckStruct ack;
        ack.received = jsonObj["received"].toInteger(0);
        ack.total = jsonObj["total"].toInteger(0);

        emit _tcpMgr.sig_offline_ack(ack);
        return;
    }
}

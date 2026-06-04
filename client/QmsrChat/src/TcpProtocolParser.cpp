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
    if (req_type == RequestType::MSG_CHAT_IMAGE)
    {
        qmsrchat::ImageMsg imgMsg;
        if (!imgMsg.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse ImageMsg from Protobuf";
            return;
        }
        ChatImageStruct msg;
        msg.from_uid = imgMsg.from_uid();
        msg.to_uid = imgMsg.to_uid();
        msg.image_id = QString::fromStdString(imgMsg.image_id());
        msg.caption = QString::fromStdString(imgMsg.caption());
        msg.timestamp = imgMsg.timestamp();
        msg.width = imgMsg.width();
        msg.height = imgMsg.height();
        msg.ext = QString::fromStdString(imgMsg.ext());
        msg.size = imgMsg.size();
        msg.md5 = QString::fromStdString(imgMsg.md5());
        emit _tcpMgr.sigChatImage(msg);
        return;
    }
    if (req_type == RequestType::MSG_IMAGE_DOWNLOAD_RSP)
    {
        qmsrchat::ImageDownloadRsp rsp;
        if (!rsp.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse ImageDownloadRsp from Protobuf";
            return;
        }
        ImageDownloadRspStruct r;
        r.error = rsp.error();
        r.image_id = QString::fromStdString(rsp.image_id());
        r.offset = rsp.offset();
        emit _tcpMgr.sigImageDownloadRsp(r);
        return;
    }
    if (req_type == RequestType::MSG_CHAT_RECALL)
    {
        qmsrchat::EditAck ack;
        if (!ack.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse EditAck (recall ack) from Protobuf";
            return;
        }
        ChatEditAckStruct a;
        a.error = ack.error();
        a.message = QString::fromStdString(ack.message());
        a.msg_timestamp = ack.msg_timestamp();
        a.edit_ts = ack.edit_ts();
        emit _tcpMgr.sigChatRecallRsp(a);
        return;
    }
    if (req_type == RequestType::MSG_CHAT_EDIT)
    {
        qmsrchat::EditAck ack;
        if (!ack.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse EditAck from Protobuf";
            return;
        }
        ChatEditAckStruct a;
        a.error = ack.error();
        a.message = QString::fromStdString(ack.message());
        a.msg_timestamp = ack.msg_timestamp();
        a.edit_ts = ack.edit_ts();
        emit _tcpMgr.sigChatEditAck(a);
        return;
    }
    if (req_type == RequestType::MSG_CHAT_RECALL_NOTIFY)
    {
        qmsrchat::RecallNotify n;
        if (!n.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse RecallNotify from Protobuf";
            return;
        }
        ChatRecallNotifyStruct notify;
        notify.msg_timestamp = n.msg_timestamp();
        notify.recall_uid = n.recall_uid();
        notify.recalled_to = n.recalled_to();
        notify.recall_ts = n.recall_ts();
        emit _tcpMgr.sigChatRecallNotify(notify);
        return;
    }
    if (req_type == RequestType::MSG_CHAT_EDIT_NOTIFY)
    {
        qmsrchat::EditNotify n;
        if (!n.ParseFromArray(data.constData(), data.size()))
        {
            qWarning() << "Failed to parse EditNotify from Protobuf";
            return;
        }
        ChatEditNotifyStruct notify;
        notify.msg_timestamp = n.msg_timestamp();
        notify.from_uid = n.from_uid();
        notify.new_content = QString::fromStdString(n.new_content());
        notify.edit_ts = n.edit_ts();
        emit _tcpMgr.sigChatEditNotify(notify);
        return;
    }
}

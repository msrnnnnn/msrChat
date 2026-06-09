/**
 * @file    TcpProtocolParser.cpp
 * @brief   TCP 协议数据解析器实现
 * @details 负责将原始字节流解析为业务结构体，通过信号通知 TcpMgr。
 *          登录/注册等请求使用 JSON 格式，聊天消息使用 Protobuf 格式。
 */
#include "TcpProtocolParser.h"
#include "Message.pb.h"
#include "ProtocolStructs.h"
#include "TcpMgr.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

/**
 * @brief 解析登录/注册相关数据包（JSON 格式）
 * @param req_type 请求类型
 * @param data JSON 字节数组
 * @details 兼容 ID_LOGIN_USER、ID_GET_VERIFY_CODE、ID_REGISTER_USER、ID_RESET_PWD 四种类型。
 */
void TcpProtocolParser::parseLoginPacket(RequestType req_type, const QByteArray &data)
{
    // 解析 JSON 数据
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

        emit _tcpMgr.sigLoginRsp(rsp);
    }
    else if (req_type == RequestType::ID_GET_VERIFY_CODE)
    {
        VerifyCodeRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.email = jsonObj["email"].toString();
        rsp.code = jsonObj["code"].toInt();

        emit _tcpMgr.sigVerifyCodeRsp(rsp);
    }
    else if (req_type == RequestType::ID_REGISTER_USER)
    {
        RegisterRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();
        rsp.email = jsonObj["email"].toString();

        emit _tcpMgr.sigRegisterRsp(rsp);
    }
    else if (req_type == RequestType::ID_RESET_PWD)
    {
        ResetPwdRspStruct rsp;
        rsp.error = jsonObj["error"].toInt();

        emit _tcpMgr.sigResetPwdRsp(rsp);
    }
}

/**
 * @brief 解析聊天消息数据包
 * @param req_type 请求类型
 * @param data 原始数据（JSON 或 Protobuf 格式）
 * @details 聊天登录使用 JSON 格式，其余消息（文本、ACK、图片、撤回、编辑等）使用 Protobuf 反序列化。
 */
void TcpProtocolParser::parseChatPacket(RequestType req_type, const QByteArray &data)
{
    // MSG_CHAT_LOGIN 使用 JSON 格式，其余聊天消息使用 Protobuf 格式
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

        emit _tcpMgr.sigChatLoginRsp(rsp);
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

            emit _tcpMgr.sigChatTextMsg(msg);
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

        emit _tcpMgr.sigChatAck(ack);
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

        emit _tcpMgr.sigOfflineAck(ack);
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
        a.new_content = QString::fromStdString(ack.new_content());
        emit _tcpMgr.sigChatEditAck(a);
        return;
    }
    // MSG_CHAT_RECALL_NOTIFY：服务器主动推送的撤回通知（不同于客户端请求撤回后的 ACK）
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
    // MSG_CHAT_EDIT_NOTIFY：服务器主动推送的编辑通知（通知其他会话参与者消息已被编辑）
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

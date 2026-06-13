/**
 * @file    MessageRouter.cpp
 * @brief   消息路由实现
 * @details 将 JSON 格式聊天消息封装为 ServerChatMsg protobuf 后转发给在线目标用户，
 *          支持按 UID 查找会话并直接发送原始消息。
 */

#include "MessageRouter.h"
#include "CSession.h"
#include "Message.pb.h"
#include "const.h"
#include <chrono>
#include <nlohmann/json.hpp>

std::atomic<int64_t> MessageRouter::_next_server_msg_id{1};

/**
 * @brief 转发聊天消息给目标用户
 * @param target_uid 目标用户 ID
 * @param msg_data JSON 消息数据
 * @return 是否发送成功
 * @details 将 JSON 转换为 Protobuf ServerChatMsg 后发送
 */
bool MessageRouter::ForwardMessage(int target_uid, const std::string &msg_data)
{
    auto session = SessionManager::Instance().GetSession(target_uid);
    if (!session)
    {
        return false;
    }

    auto json_data = nlohmann::json::parse(msg_data, nullptr, false);
    if (json_data.is_discarded())
    {
        return false;
    }

    qmsrchat::ServerChatMsg chat_msg;
    chat_msg.set_from_uid(json_data.value("from_uid", 0));
    chat_msg.set_to_uid(json_data.value("to_uid", target_uid));
    chat_msg.set_content(json_data.value("content", ""));
    chat_msg.set_server_msg_id(_next_server_msg_id.fetch_add(1));
    chat_msg.set_client_msg_id(json_data.value("client_msg_id", ""));
    chat_msg.set_timestamp(json_data.value("timestamp",
        static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count())));

    std::string serialized;
    if (!chat_msg.SerializeToString(&serialized))
    {
        return false;
    }

    session->Send(serialized, MSG_CHAT_TEXT);
    return true;
}

/**
 * @brief 向指定会话发送原始消息
 * @param session 目标会话智能指针
 * @param msg_data 消息数据
 * @param msg_id 消息协议号
 * @return 发送成功返回 true
 */
bool MessageRouter::SendToSession(const std::shared_ptr<CSession> &session, const std::string &msg_data, short msg_id)
{
    if (!session)
    {
        return false;
    }

    try
    {
        session->Send(msg_data, msg_id);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

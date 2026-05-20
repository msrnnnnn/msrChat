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
    chat_msg.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());

    std::string serialized;
    if (!chat_msg.SerializeToString(&serialized))
    {
        return false;
    }

    session->Send(serialized, MSG_CHAT_TEXT);
    return true;
}

/**
 * @brief 广播消息给所有在线用户
 * @param msg_data 消息数据
 * @param exclude_uid 排除的用户 ID（可选）
 * @return 是否全部发送成功
 */
bool MessageRouter::BroadcastMessage(const std::string &msg_data, int exclude_uid)
{
    auto json_data = nlohmann::json::parse(msg_data, nullptr, false);
    if (json_data.is_discarded())
    {
        return false;
    }

    qmsrchat::ServerChatMsg broadcast_msg;
    broadcast_msg.set_from_uid(json_data.value("from_uid", 0));
    broadcast_msg.set_to_uid(0);
    broadcast_msg.set_content(json_data.value("content", ""));
    broadcast_msg.set_client_msg_id(json_data.value("client_msg_id", ""));
    broadcast_msg.set_server_msg_id(_next_server_msg_id.fetch_add(1));
    broadcast_msg.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());

    std::string serialized;
    if (!broadcast_msg.SerializeToString(&serialized))
    {
        return false;
    }

    bool all_sent = true;
    SessionManager::Instance().ForEachSession([&](int uid, std::shared_ptr<CSession> session) {
        if (uid != exclude_uid)
        {
            try
            {
                session->Send(serialized, MSG_CHAT_TEXT);
            }
            catch (...)
            {
                all_sent = false;
            }
        }
    });
    return all_sent;
}

bool MessageRouter::SendToSession(std::shared_ptr<CSession> session, const std::string &msg_data, short msg_id)
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

bool MessageRouter::SendBinaryToSession(std::shared_ptr<CSession> session, const std::string &json_data,
                                        const std::vector<char> &binary_data, short msg_id)
{
    if (!session)
    {
        return false;
    }

    try
    {
        session->SendBinary(json_data, binary_data, msg_id);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

#include "MessageRouter.h"
#include "CSession.h"
#include "Message.pb.h"
#include "const.h"
#include <chrono>
#include <nlohmann/json.hpp>

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

bool MessageRouter::BroadcastMessage(const std::string &msg_data, int exclude_uid)
{
    bool all_sent = true;
    SessionManager::Instance().ForEachSession([&](int uid, std::shared_ptr<CSession> session) {
        if (uid != exclude_uid)
        {
            try
            {
                session->Send(msg_data, 0);
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

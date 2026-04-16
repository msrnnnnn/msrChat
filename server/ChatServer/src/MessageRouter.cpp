#include "MessageRouter.h"
#include "CSession.h"

bool MessageRouter::ForwardMessage(int target_uid, const std::string &msg_data)
{
    auto session = SessionManager::Instance().GetSession(target_uid);
    if (!session)
    {
        return false;
    }

    session->Send(msg_data, 0);
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

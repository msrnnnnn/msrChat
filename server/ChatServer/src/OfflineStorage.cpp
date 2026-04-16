#include "OfflineStorage.h"
#include "CSession.h"

void OfflineStorage::StoreMessage(int target_uid, const std::string &msg_data)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _offline_messages[target_uid].push_back(msg_data);
}

void OfflineStorage::SendOfflineMessages(int uid, std::shared_ptr<CSession> session)
{
    std::vector<std::string> messages;
    {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        auto it = _offline_messages.find(uid);
        if (it != _offline_messages.end())
        {
            messages = std::move(it->second);
            _offline_messages.erase(it);
        }
    }

    for (const auto &msg : messages)
    {
        MessageRouter::Instance().SendToSession(session, msg, 0);
    }
}

std::vector<std::string> OfflineStorage::GetOfflineMessages(int uid) const
{
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _offline_messages.find(uid);
    if (it != _offline_messages.end())
    {
        return it->second;
    }
    return {};
}

void OfflineStorage::ClearOfflineMessages(int uid)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _offline_messages.erase(uid);
}

std::size_t OfflineStorage::GetOfflineMessageCount(int uid) const
{
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _offline_messages.find(uid);
    if (it != _offline_messages.end())
    {
        return it->second.size();
    }
    return 0;
}

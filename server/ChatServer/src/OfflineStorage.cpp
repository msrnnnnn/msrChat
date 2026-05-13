/**
 * @file OfflineStorage.cpp
 * @brief 内存缓存式离线消息存储
 * @details 按 UID 分组存储离线消息，SendOfflineMessages 时移出缓存并投递。
 */
#include "OfflineStorage.h"
#include "CSession.h"

/**
 * @brief 存储离线消息
 * @param target_uid 目标用户 ID
 * @param msg_data 消息 JSON 数据
 */
void OfflineStorage::StoreMessage(int target_uid, const std::string &msg_data)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _offline_messages[target_uid].push_back(msg_data);
}

/**
 * @brief 发送离线消息给用户并清空缓存
 * @param uid 用户 ID
 * @param session 目标会话
 */
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

/**
 * @brief 获取用户所有离线消息（只读）
 * @param uid 用户 ID
 * @return 消息列表
 */
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

/**
 * @brief 清空用户离线消息
 * @param uid 用户 ID
 */
void OfflineStorage::ClearOfflineMessages(int uid)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _offline_messages.erase(uid);
}

/**
 * @brief 获取离线消息数量
 * @param uid 用户 ID
 * @return 消息条数
 */
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

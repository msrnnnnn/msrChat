/**
 * @file SessionManager.cpp
 * @brief 会话管理器实现
 * @details 按 UID 和 UUID 双索引存储会话，支持分片锁并发访问。
 */
#include "SessionManager.h"
#include "CSession.h"

/**
 * @brief 添加会话到管理器
 * @param uid 用户 ID
 * @param session 会话智能指针
 * @details 同时注册到 _uid_sessions 和 _uuid_sessions 两个索引
 */
void SessionManager::AddSession(int uid, std::shared_ptr<CSession> session)
{
    if (!session)
    {
        return;
    }

    std::string uuid = session->GetUuid();
    _uid_sessions.Insert(uid, session);
    _uuid_sessions.Insert(uuid, session);
}

/**
 * @brief 通过 UID 移除会话
 * @param uid 用户 ID
 * @details 从两个索引中同步移除对应会话
 */
void SessionManager::RemoveSession(int uid)
{
    auto session = _uid_sessions.Find(uid);
    if (session)
    {
        std::string uuid = (*session)->GetUuid();
        _uuid_sessions.Erase(uuid);
    }
    _uid_sessions.Erase(uid);
}

/**
 * @brief 通过 UUID 移除会话
 * @param uuid 会话 UUID
 * @details 根据 UUID 查到的 UID 再从 _uid_sessions 中移除
 */
void SessionManager::RemoveSessionByUuid(const std::string &uuid)
{
    auto session = _uuid_sessions.Find(uuid);
    if (session)
    {
        int uid = (*session)->GetUserUid();
        _uid_sessions.Erase(uid);
    }
    _uuid_sessions.Erase(uuid);
}

/**
 * @brief 通过 UID 获取会话
 * @param uid 用户 ID
 * @return 会话智能指针，不存在则返回 nullptr
 */
std::shared_ptr<CSession> SessionManager::GetSession(int uid) const
{
    auto *session = _uid_sessions.Find(uid);
    return session ? *session : nullptr;
}

/**
 * @brief 通过 UUID 获取会话
 * @param uuid 会话 UUID
 * @return 会话智能指针，不存在则返回 nullptr
 */
std::shared_ptr<CSession> SessionManager::GetSessionByUuid(const std::string &uuid) const
{
    auto *session = _uuid_sessions.Find(uuid);
    return session ? *session : nullptr;
}

/**
 * @brief 获取当前会话总数
 * @return 会话总数
 * @details 遍历所有分片累计计数
 */
std::size_t SessionManager::SessionCount() const
{
    std::size_t total = 0;
    for (std::size_t i = 0; i < _uid_sessions.ShardCount(); ++i)
    {
        auto lock = _uid_sessions.GetLock(i);
        total += _uid_sessions.GetShard(i).size();
    }
    return total;
}

/**
 * @brief 清空所有会话
 */
void SessionManager::ClearAll()
{
    _uid_sessions.Clear();
    _uuid_sessions.Clear();
}

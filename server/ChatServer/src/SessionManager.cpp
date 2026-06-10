/**
 * @file SessionManager.cpp
 * @brief 会话管理器实现
 * @details 按 UID 和 UUID 双索引存储会话，支持分片锁并发访问。
 */
#include "SessionManager.h"
#include "CSession.h"
#include "const.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <utility>

/**
 * @brief 添加或替换用户会话
 * @param uid 用户 ID
 * @param session 会话智能指针（所有权转移至此）
 * @details 若用户已有旧连接，先关闭旧连接；同步更新 UID 和 UUID 两个索引
 */
void SessionManager::AddSession(int uid, std::shared_ptr<CSession> session)
{
    if (!session)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(_add_mutex);

    std::string uuid = session->GetUuid();
    RemoveSessionByUuid(uuid);

    if (uid <= 0)
    {
        // 未认证会话仅注册 UUID 索引
        _uuid_sessions.Insert(uuid, std::move(session));
        spdlog::debug("[SessionManager] Unauthenticated session {} added.", uuid);
        return;
    }

    auto old_session = GetSession(uid);
    if (old_session != nullptr && old_session != session)
    {
        spdlog::info("[SessionManager] User {} has existing session, closing old connection.", uid);
        // 发送踢出通知
        nlohmann::json kick{{"error", ERR_KICKED}, {"message", "logged in from another device"}};
        old_session->Send(kick.dump(), 0);
        old_session->Close();
    }

    _uuid_sessions.Insert(uuid, session);
    _uid_sessions.Insert(uid, std::move(session));
    spdlog::info("[SessionManager] User {} session added.", uid);
}

/**
 * @brief 通过 UID 移除会话
 * @param uid 用户 ID
 * @details 从两个索引中同步移除对应会话
 */
void SessionManager::RemoveSession(int uid)
{
    std::string uuid_to_erase;
    _uid_sessions.RemoveIfMatch(uid, [&](const std::shared_ptr<CSession> &session) {
        uuid_to_erase = session->GetUuid();
        return true;
    });
    if (!uuid_to_erase.empty())
    {
        _uuid_sessions.Erase(uuid_to_erase);
    }
}

/**
 * @brief 通过 UUID 移除会话
 * @param uuid 会话 UUID
 * @details 根据 UUID 查到的 UID 再从 _uid_sessions 中移除
 */
void SessionManager::RemoveSessionByUuid(const std::string &uuid)
{
    int uid_to_erase = -1;
    _uuid_sessions.RemoveIfMatch(uuid, [&](const std::shared_ptr<CSession> &session) {
        uid_to_erase = session->GetUserUid();
        return true;
    });
    if (uid_to_erase != -1)
    {
        _uid_sessions.Erase(uid_to_erase);
    }
}

/**
 * @brief 通过 UID 获取会话
 * @param uid 用户 ID
 * @return 会话智能指针，不存在则返回 nullptr
 */
std::shared_ptr<CSession> SessionManager::GetSession(int uid) const
{
    auto session = _uid_sessions.Find(uid);
    return session.value_or(nullptr);
}

/**
 * @brief 清空所有会话
 */
void SessionManager::ClearAll()
{
    _uid_sessions.Clear();
    _uuid_sessions.Clear();
}

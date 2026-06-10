/**
 * @file TokenManager.cpp
 * @brief Token 管理器实现
 * @details 管理用户登录 Token 的存储与校验。
 */
#include "TokenManager.h"
#include "SQLiteMgr.h"
#include "AuthRepository.h"
#include <spdlog/spdlog.h>
#include <ctime>

/**
 * @brief 设置/更新用户 Token
 * @param uid 用户 ID
 * @param token Token 字符串
 * @details 同时写入内存缓存（CoarseMap）和 SQLite 持久化
 */
void TokenManager::SetToken(int uid, const std::string &token)
{
    int64_t now_sec = static_cast<int64_t>(std::time(nullptr));
    _uid_tokens.Insert(uid, TokenEntry{token, now_sec});
    SQLiteMgr::Instance().Auth().SaveToken(uid, token);
}

/**
 * @brief 校验用户 Token
 * @param uid 用户 ID
 * @param token 待校验的 Token 字符串
 * @return 校验成功返回 true
 * @details 先查内存缓存 CoarseMap，不存在则判定失败
 */
bool TokenManager::CheckToken(int uid, const std::string &token)
{
    auto entry = _uid_tokens.Find(uid);
    if (!entry.has_value())
    {
        spdlog::warn("[TokenManager] Token check failed for uid {}", uid);
        return false;
    }

    int64_t now_sec = static_cast<int64_t>(std::time(nullptr));
    if (now_sec - entry->created_at > TOKEN_TTL_SEC)
    {
        spdlog::warn("[TokenManager] Token expired for uid {} (age={}s)", uid, now_sec - entry->created_at);
        _uid_tokens.Erase(uid);
        return false;
    }

    bool matched = entry->token == token;
    if (!matched)
    {
        spdlog::warn("[TokenManager] Token mismatch for uid {}", uid);
    }
    return matched;
}

/**
 * @brief 从数据库批量加载 Token 到内存
 * @details 服务启动时调用，将 tokens 表所有记录载入 CoarseMap 缓存
 */
void TokenManager::LoadTokensFromDB()
{
    auto tokens = SQLiteMgr::Instance().Auth().GetAllTokens();
    int64_t now_sec = static_cast<int64_t>(std::time(nullptr));
    int expired = 0;
    for (const auto &[uid, token, created_at] : tokens)
    {
        if (now_sec - created_at > TOKEN_TTL_SEC)
        {
            expired++;
            continue;
        }
        _uid_tokens.Insert(uid, TokenEntry{token, created_at});
    }
    spdlog::info("[TokenManager] Loaded {} tokens from database ({} expired skipped)",
                 tokens.size() - expired, expired);
}

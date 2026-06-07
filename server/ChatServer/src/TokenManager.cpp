/**
 * @file TokenManager.cpp
 * @brief Token 管理器实现
 * @details 管理用户登录 Token 的存储与校验。
 */
#include "TokenManager.h"
#include "SQLiteMgr.h"
#include <spdlog/spdlog.h>

/**
 * @brief 设置/更新用户 Token
 * @param uid 用户 ID
 * @param token Token 字符串
 * @details 同时写入内存缓存（CoarseMap）和 SQLite 持久化
 */
void TokenManager::SetToken(int uid, const std::string &token)
{
    _uid_tokens.Insert(uid, token);
    SQLiteMgr::Instance().SaveToken(uid, token);
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
    auto stored_token = _uid_tokens.Find(uid);
    if (!stored_token.has_value())
    {
        spdlog::warn("[TokenManager] Token check failed for uid {}", uid);
        return false;
    }
    bool matched = stored_token.value() == token;
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
    auto tokens = SQLiteMgr::Instance().GetAllTokens();
    for (const auto &[uid, token] : tokens)
    {
        _uid_tokens.Insert(uid, token);
    }
    spdlog::info("[TokenManager] Loaded {} tokens from database", tokens.size());
}

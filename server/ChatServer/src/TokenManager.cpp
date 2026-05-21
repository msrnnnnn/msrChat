/**
 * @file TokenManager.cpp
 * @brief Token 管理器实现
 * @details 管理用户登录 Token 的存储与校验。
 */
#include "TokenManager.h"
#include "SQLiteMgr.h"
#include <spdlog/spdlog.h>

void TokenManager::SetToken(int uid, const std::string &token)
{
    _uid_tokens.Insert(uid, token);
    SQLiteMgr::Instance().SaveToken(uid, token);
}

bool TokenManager::CheckToken(int uid, const std::string &token)
{
    auto stored_token = _uid_tokens.Find(uid);
    if (!stored_token)
    {
        spdlog::warn("[TokenManager] Token check failed for uid {}", uid);
        return false;
    }
    bool matched = *stored_token == token;
    if (!matched)
    {
        spdlog::warn("[TokenManager] Token mismatch for uid {}", uid);
    }
    return matched;
}

void TokenManager::RemoveToken(int uid)
{
    _uid_tokens.Erase(uid);
    SQLiteMgr::Instance().RemoveTokenFromDB(uid);
}

std::string TokenManager::GetToken(int uid) const
{
    auto token = _uid_tokens.Find(uid);
    return token ? *token : std::string();
}

void TokenManager::LoadTokensFromDB()
{
    auto tokens = SQLiteMgr::Instance().GetAllTokens();
    for (const auto &[uid, token] : tokens)
    {
        _uid_tokens.Insert(uid, token);
    }
    spdlog::info("[TokenManager] Loaded {} tokens from database", tokens.size());
}

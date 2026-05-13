/**
 * @file TokenManager.cpp
 * @brief Token 管理器实现
 * @details 管理用户登录 Token 的存储与校验。
 */
#include "TokenManager.h"

/**
 * @brief 设置用户 Token
 * @param uid 用户 ID
 * @param token Token 值
 */
void TokenManager::SetToken(int uid, const std::string &token)
{
    _uid_tokens.Insert(uid, token);
}

/**
 * @brief 校验用户 Token
 * @param uid 用户 ID
 * @param token 待校验的 Token
 * @return 校验通过返回 true
 */
bool TokenManager::CheckToken(int uid, const std::string &token)
{
    auto stored_token = _uid_tokens.Find(uid);
    if (!stored_token)
    {
        return false;
    }
    return *stored_token == token;
}

/**
 * @brief 移除用户 Token
 * @param uid 用户 ID
 */
void TokenManager::RemoveToken(int uid)
{
    _uid_tokens.Erase(uid);
}

/**
 * @brief 获取用户 Token
 * @param uid 用户 ID
 * @return Token 值，不存在则返回空字符串
 */
std::string TokenManager::GetToken(int uid) const
{
    auto token = _uid_tokens.Find(uid);
    return token ? *token : std::string();
}

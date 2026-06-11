#pragma once
/**
 * @file TokenManager.h
 * @brief Token 管理器 —— 在内存中缓存 uid->token 映射
 * @details 启动时从数据库加载所有 Token 到内存，运行时读写均在内存中完成。
 *          使用 ShardedMap 保证并发安全。Token 持久化由 SQLiteMgr 负责。
 */
#ifndef TOKEN_MANAGER_H
#define TOKEN_MANAGER_H

#include "ShardedMap.h"
#include <cstdint>
#include <memory>
#include <string>

class CSession;

/**
 * @brief Token 缓存条目
 */
struct TokenEntry
{
    std::string token;
    int64_t created_at = 0;
};

/**
 * @brief Token 管理器（单例）
 * @details 维护 uid->token 的内存映射表，用于快速鉴权。
 *          SetToken 同时更新内存和数据库；CheckToken 仅读内存。
 */
class TokenManager
{
public:
    static TokenManager &Instance()
    {
        static TokenManager instance;
        return instance;
    }

    void SetToken(int uid, const std::string &token);
    bool CheckToken(int uid, const std::string &token);
    void LoadTokensFromDB();

private:
    TokenManager() = default;
    ~TokenManager() = default;

    TokenManager(const TokenManager &) = delete;
    TokenManager &operator=(const TokenManager &) = delete;

    TokenManager(TokenManager &&) = delete;
    TokenManager &operator=(TokenManager &&) = delete;

    ShardedMap<int, TokenEntry> _uid_tokens{16};
    static constexpr int64_t TOKEN_TTL_SEC = 7 * 24 * 3600;  ///< Token 有效期 7 天
};

#endif

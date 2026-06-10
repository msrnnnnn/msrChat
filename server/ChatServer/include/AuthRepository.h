#pragma once
/**
 * @file AuthRepository.h
 * @brief 认证数据仓库 —— 用户注册/登录、验证码、Token 持久化
 * @details 从 SQLiteMgr 拆分（Phase 5D），负责所有认证相关的数据库操作。
 *          通过构造函数注入 SQLiteConnectionPool，不依赖单例。
 */
#ifndef AUTH_REPOSITORY_H
#define AUTH_REPOSITORY_H

#include "SQLiteMgr.h"  // SQLiteConnectionPool, SQLiteConnectionGuard, ScopedStmt, struct 定义
#include <optional>
#include <string>
#include <vector>

class AuthRepository
{
public:
    explicit AuthRepository(std::shared_ptr<SQLiteConnectionPool> pool);

    // === 用户认证 ===
    AuthResult RegisterUser(const std::string &username,
                            const std::string &password_hash,
                            const std::string &email);
    AuthResult LoginUser(const std::string &username,
                         const std::string &password_hash);

    // === 验证码 ===
    bool SendVerifyCode(const std::string &email, int &out_code);
    int  CheckVerifyCode(const std::string &email, const std::string &code);
    int  ResetPassword(const std::string &username, const std::string &email,
                       const std::string &code,
                       const std::string &new_password_hash);

    // === 用户查询 ===
    std::optional<User> GetUserByUsername(const std::string &username);

    // === Token 持久化 ===
    bool SaveToken(int uid, const std::string &token);
    std::optional<std::string> GetTokenFromDB(int uid);
    std::vector<TokenRecord> GetAllTokens();

private:
    std::optional<User> GetUserByUsernameUnlocked(sqlite3 *db,
                                                  const std::string &username);

    std::shared_ptr<SQLiteConnectionPool> _pool;
};

#endif // AUTH_REPOSITORY_H

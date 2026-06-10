/**
 * @file AuthRepository.cpp
 * @brief 认证数据仓库实现
 * @details 从 SQLiteMgr.cpp 拆分（Phase 5D），包含用户注册/登录、验证码、
 *          Token 持久化及密码学辅助函数。
 */
#include "AuthRepository.h"
#include "const.h"
#include <cstring>
#include <ctime>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>

// ============================================================
// 密码学辅助函数（文件内部）
// ============================================================

static std::string SHA256Hash(const std::string &input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(), hash);
    char hex_str[2 * SHA256_DIGEST_LENGTH + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
        sprintf(hex_str + i * 2, "%02x", hash[i]);
    }
    return std::string(hex_str, 2 * SHA256_DIGEST_LENGTH);
}

static std::string SecureRandomHex(int bytes)
{
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), bytes) != 1)
    {
        std::random_device rd;
        for (int i = 0; i < bytes; ++i) buf[i] = static_cast<unsigned char>(rd());
    }
    std::stringstream ss;
    for (int i = 0; i < bytes; ++i)
    {
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(buf[i]);
    }
    return ss.str();
}

static std::string GenerateSalt()
{
    return SecureRandomHex(16);
}

static std::string PBKDF2_SHA256(const std::string &password, const std::string &salt, int iterations = 10000)
{
    constexpr size_t hash_len = 32;
    unsigned char hash[hash_len];
    PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                      reinterpret_cast<const unsigned char *>(salt.c_str()), static_cast<int>(salt.size()),
                      iterations, EVP_sha256(), static_cast<int>(hash_len), hash);
    char hex_str[2 * hash_len + 1];
    for (size_t i = 0; i < hash_len; ++i)
        sprintf(hex_str + i * 2, "%02x", hash[i]);
    return std::string(hex_str, 2 * hash_len);
}

// ============================================================
// AuthRepository 实现
// ============================================================

AuthRepository::AuthRepository(std::shared_ptr<SQLiteConnectionPool> pool)
    : _pool(std::move(pool))
{
}

AuthResult AuthRepository::RegisterUser(
    const std::string &username, const std::string &password_hash, const std::string &email)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        AuthResult r;
        r.error = ERR_DB;
        return r;
    }
    sqlite3 *db = guard.Get();

    auto existing = GetUserByUsernameUnlocked(db, username);
    if (existing.has_value())
    {
        AuthResult r;
        r.error = ERR_USER_EXIST;
        return r;
    }

    // Phase 3 — PBKDF2 密码哈希: pbkdf2$salt$hash
    std::string salt = GenerateSalt();
    std::string pbkdf2_hash = PBKDF2_SHA256(password_hash, salt);
    std::string stored_password = "pbkdf2$" + salt + "$" + pbkdf2_hash;

    ScopedStmt stmt(
        db, "INSERT INTO users (username, password_hash, email, avatar_path, created_at) VALUES (?, ?, ?, '', ?)");
    if (!stmt)
    {
        AuthResult r;
        r.error = ERR_DB;
        return r;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, stored_password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, time(nullptr));

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        AuthResult r;
        r.error = ERR_DB;
        return r;
    }

    int64_t uid = sqlite3_last_insert_rowid(db);

    AuthResult r;
    r.error = 0;
    r.uid = static_cast<int>(uid);
    r.token = SecureRandomHex(32);
    r.username = username;
    return r;
}

AuthResult AuthRepository::LoginUser(const std::string &username, const std::string &password_hash)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        AuthResult r;
        r.error = ERR_DB;
        return r;
    }
    sqlite3 *db = guard.Get();

    auto user = GetUserByUsernameUnlocked(db, username);
    if (!user.has_value())
    {
        AuthResult r;
        r.error = ERR_USER_NOT_EXIST;
        return r;
    }

    // 解析存储的密码格式
    std::string stored = user->password_hash;
    bool need_upgrade = false;

    if (stored.compare(0, 7, "pbkdf2$") == 0)
    {
        // Phase 3 — PBKDF2 格式: pbkdf2$salt$hash
        std::string rest = stored.substr(7);
        auto sep = rest.find('$');
        if (sep == std::string::npos)
        {
            AuthResult r; r.error = ERR_DB; return r;
        }
        std::string salt = rest.substr(0, sep);
        std::string expected_hash = PBKDF2_SHA256(password_hash, salt);
        std::string stored_hash = rest.substr(sep + 1);
        if (expected_hash != stored_hash)
        {
            AuthResult r; r.error = ERR_PASSWD_ERR; return r;
        }
    }
    else if (stored.find('$') != std::string::npos)
    {
        // 旧格式: salt$sha256_hash → 验证后透明升级
        auto dollar_pos = stored.find('$');
        std::string salt = stored.substr(0, dollar_pos);
        std::string expected_hash = SHA256Hash(password_hash + salt);
        std::string stored_hash = stored.substr(dollar_pos + 1);
        if (expected_hash != stored_hash)
        {
            AuthResult r; r.error = ERR_PASSWD_ERR; return r;
        }
        need_upgrade = true;
    }
    else
    {
        // 旧版明文密码 → 验证后透明升级
        if (stored != password_hash)
        {
            AuthResult r; r.error = ERR_PASSWD_ERR; return r;
        }
        need_upgrade = true;
    }

    // 透明升级：将旧格式密码更新为 PBKDF2
    if (need_upgrade)
    {
        std::string new_salt = GenerateSalt();
        std::string new_hash = PBKDF2_SHA256(password_hash, new_salt);
        std::string new_password = "pbkdf2$" + new_salt + "$" + new_hash;
        ScopedStmt upd(db, "UPDATE users SET password_hash = ? WHERE uid = ?");
        if (upd)
        {
            sqlite3_bind_text(upd, 1, new_password.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(upd, 2, user->uid);
            sqlite3_step(upd);
        }
        spdlog::info("[AuthRepository] Transparently upgraded password for uid {}", user->uid);
    }

    AuthResult r;
    r.error = 0;
    r.uid = user->uid;
    r.token = SecureRandomHex(32);
    r.username = user->username;
    return r;
}

bool AuthRepository::SendVerifyCode(const std::string &email, int &out_code)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    const int code = []() {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(100000, 999999);
        return dist(rng);
    }();

    spdlog::debug("[AuthRepository] Generated verify code for {}: {}", email, code);
    out_code = code;

    ScopedStmt del_stmt(db, "DELETE FROM verify_codes WHERE email = ?");
    if (del_stmt)
    {
        sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(del_stmt) != SQLITE_DONE)
        {
            spdlog::warn("[AuthRepository] Failed to delete old verify codes for {}", email);
        }
    }

    ScopedStmt ins_stmt(db, "INSERT INTO verify_codes (email, code, created_at, expires_at) VALUES (?, ?, ?, ?)");
    if (!ins_stmt)
    {
        return false;
    }

    int64_t now = time(nullptr);
    sqlite3_bind_text(ins_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(ins_stmt, 2, code);
    sqlite3_bind_int64(ins_stmt, 3, now);
    sqlite3_bind_int64(ins_stmt, 4, now + VERIFY_CODE_EXPIRY_SEC);

    spdlog::debug("[Auth] VerifyCode for {}: {}", email, code);

    return sqlite3_step(ins_stmt) == SQLITE_DONE;
}

int AuthRepository::CheckVerifyCode(const std::string &email, const std::string &code)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return ERR_VERIFY_EXPIRED;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "SELECT expires_at FROM verify_codes WHERE email = ? AND code = ? ORDER BY id DESC LIMIT 1");
    if (!stmt)
    {
        return ERR_VERIFY_EXPIRED;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int64_t expires_at = sqlite3_column_int64(stmt, 0);

        if (time(nullptr) > expires_at)
        {
            return ERR_VERIFY_EXPIRED;
        }
        return 0;
    }

    return ERR_VERIFY_WRONG;
}

int AuthRepository::ResetPassword(
    const std::string &username, const std::string &email, const std::string &code,
    const std::string &new_password_hash)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return ERR_DB;
    }
    sqlite3 *db = guard.Get();

    auto user = GetUserByUsernameUnlocked(db, username);
    if (!user.has_value())
    {
        return ERR_USER_NOT_EXIST;
    }

    if (user->email != email)
    {
        return ERR_EMAIL_NOT_MATCH;
    }

    int verify_result = CheckVerifyCode(email, code);
    if (verify_result != 0)
    {
        return verify_result;
    }

    std::string salt = GenerateSalt();
    std::string pbkdf2_hash = PBKDF2_SHA256(new_password_hash, salt);
    std::string stored_password = "pbkdf2$" + salt + "$" + pbkdf2_hash;

    ScopedStmt stmt(db, "UPDATE users SET password_hash = ? WHERE uid = ?");
    if (!stmt)
    {
        return ERR_PASSWD_UPDATE;
    }

    sqlite3_bind_text(stmt, 1, stored_password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user->uid);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        return ERR_PASSWD_UPDATE;
    }

    ScopedStmt del_stmt(db, "DELETE FROM verify_codes WHERE email = ?");
    if (del_stmt)
    {
        sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(del_stmt);
    }

    return 0;
}

std::optional<User> AuthRepository::GetUserByUsername(const std::string &username)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
        return std::nullopt;
    return GetUserByUsernameUnlocked(guard.Get(), username);
}

std::optional<User> AuthRepository::GetUserByUsernameUnlocked(sqlite3 *db, const std::string &username)
{
    ScopedStmt stmt(
        db, "SELECT uid, username, password_hash, email, avatar_path, created_at FROM users WHERE username = ?");
    if (!stmt)
    {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        User user;
        user.uid = sqlite3_column_int(stmt, 0);
        user.username = SafeColumnText(stmt, 1);
        user.password_hash = SafeColumnText(stmt, 2);
        user.email = SafeColumnText(stmt, 3);
        user.avatar_path = SafeColumnText(stmt, 4);
        user.created_at = sqlite3_column_int64(stmt, 5);
        return user;
    }
    return std::nullopt;
}

bool AuthRepository::SaveToken(int uid, const std::string &token)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    ScopedStmt stmt(db, "INSERT OR REPLACE INTO tokens (uid, token, created_at) VALUES (?, ?, ?)");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_text(stmt, 2, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, time(nullptr));
    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::optional<std::string> AuthRepository::GetTokenFromDB(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return std::nullopt;
    sqlite3 *db = guard.Get();
    ScopedStmt stmt(db, "SELECT token FROM tokens WHERE uid = ?");
    if (!stmt) return std::nullopt;
    sqlite3_bind_int(stmt, 1, uid);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        return SafeColumnText(stmt, 0);
    }
    return std::nullopt;
}

std::vector<TokenRecord> AuthRepository::GetAllTokens()
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return {};
    sqlite3 *db = guard.Get();
    std::vector<TokenRecord> tokens;
    ScopedStmt stmt(db, "SELECT uid, token, created_at FROM tokens");
    if (!stmt) return tokens;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        TokenRecord rec;
        rec.uid = sqlite3_column_int(stmt, 0);
        rec.token = SafeColumnText(stmt, 1);
        rec.created_at = sqlite3_column_int64(stmt, 2);
        tokens.push_back(rec);
    }
    return tokens;
}

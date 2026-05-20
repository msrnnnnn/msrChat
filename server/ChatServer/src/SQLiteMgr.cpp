/**
 * @file SQLiteMgr.cpp
 * @brief SQLite 数据库管理实现
 * @details 包含连接池、用户认证、消息存储、验证码管理。
 */
#include "SQLiteMgr.h"
#include <cstring>
#include <ctime>
#include <iomanip>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>

static std::string SHA256(const std::string &input)
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

SQLiteConnectionPool::SQLiteConnectionPool(const std::string &db_path, int pool_size)
    : _db_path(db_path),
      _pool_size(pool_size)
{
}

SQLiteConnectionPool::~SQLiteConnectionPool()
{
    Shutdown();
}

/**
 * @brief 初始化数据库连接
 * @param db 输出：sqlite3 指针引用
 * @return 初始化成功返回 true
 * @details 设置 WAL 模式和非同步级别
 */
bool SQLiteConnectionPool::InitializeConnection(sqlite3 **db)
{
    if (sqlite3_open(_db_path.c_str(), db) != SQLITE_OK)
    {
        spdlog::error("[SQLitePool] Failed to open database: {}", _db_path);
        return false;
    }

    char *err_msg = nullptr;

    if (sqlite3_exec(*db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        spdlog::error("[SQLitePool] Failed to set WAL mode: {}", err_msg ? err_msg : "unknown");
        if (err_msg)
            sqlite3_free(err_msg);
        sqlite3_close(*db);
        *db = nullptr;
        return false;
    }

    if (sqlite3_exec(*db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        spdlog::error("[SQLitePool] Failed to set synchronous mode: {}", err_msg ? err_msg : "unknown");
        if (err_msg)
            sqlite3_free(err_msg);
        sqlite3_close(*db);
        *db = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief 从连接池获取一个连接
 * @return 可用连接，不可用时返回 nullptr
 * @details 阻塞直到有可用连接或池已关闭
 */
std::shared_ptr<SQLiteConnection> SQLiteConnectionPool::Acquire()
{
    std::unique_lock<std::mutex> lock(_mutex);

    _cv.wait(lock, [this] { return !_available_connections.empty() || _shutdown.load(); });

    if (_shutdown.load())
    {
        return nullptr;
    }

    auto conn = _available_connections.front();
    _available_connections.pop();
    conn->SetInUse(true);

    return conn;
}

/**
 * @brief 归还连接到池
 * @param conn 要归还的连接
 */
void SQLiteConnectionPool::Release(std::shared_ptr<SQLiteConnection> conn)
{
    if (!conn)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    conn->SetInUse(false);
    _available_connections.push(conn);
    _cv.notify_one();
}

/**
 * @brief 关闭连接池
 * @details 设置关闭标志、唤醒所有等待线程、关闭所有连接
 */
void SQLiteConnectionPool::Shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_shutdown.load())
    {
        return;
    }

    _shutdown.store(true);
    _cv.notify_all();

    CloseAllConnections();

    _initialized.store(false);
}

/**
 * @brief 关闭所有连接
 */
void SQLiteConnectionPool::CloseAllConnections()
{
    for (auto &conn : _all_connections)
    {
        if (conn && conn->Get())
        {
            sqlite3_close(conn->Get());
        }
    }
    _all_connections.clear();

    while (!_available_connections.empty())
    {
        _available_connections.pop();
    }
}

SQLiteMgr::~SQLiteMgr()
{
    Shutdown();
}

SQLiteMgr &SQLiteMgr::Instance()
{
    static SQLiteMgr instance;
    return instance;
}

/**
 * @brief 初始化管理器
 * @param db_path 数据库文件路径
 * @param pool_size 连接池大小
 * @return 成功返回 true
 */
bool SQLiteMgr::Init(const std::string &db_path, int pool_size)
{
    if (_initialized.load())
    {
        return true;
    }

    _pool = std::make_shared<SQLiteConnectionPool>(db_path, pool_size);

    sqlite3 *db_temp = nullptr;
    if (!_pool->InitializeConnection(&db_temp))
    {
        return false;
    }

    if (!CreateTables(db_temp))
    {
        sqlite3_close(db_temp);
        return false;
    }

    sqlite3_close(db_temp);

    for (int i = 0; i < pool_size; ++i)
    {
        sqlite3 *db = nullptr;
        if (!_pool->InitializeConnection(&db))
        {
            spdlog::error("[SQLiteMgr] Failed to initialize connection {} in pool", i);
            continue;
        }

        if (!CreateTables(db))
        {
            spdlog::error("[SQLiteMgr] Failed to setup tables for connection {} in pool", i);
            sqlite3_close(db);
            continue;
        }

        auto conn = std::make_shared<SQLiteConnection>(db);
        _pool->_available_connections.push(conn);
        _pool->_all_connections.push_back(conn);
    }

    if (_pool->_available_connections.empty())
    {
        spdlog::error("[SQLiteMgr] No connections available in pool");
        return false;
    }

    spdlog::info("[SQLiteMgr] Connection pool initialized with {} connections", _pool->_all_connections.size());

    _initialized.store(true);
    return true;
}

/**
 * @brief 关闭管理器，释放连接池
 */
void SQLiteMgr::Shutdown()
{
    if (!_initialized.load())
    {
        return;
    }

    if (_pool)
    {
        _pool->Shutdown();
    }

    _pool.reset();
    _initialized.store(false);
}

/**
 * @brief 创建数据库表
 * @param db sqlite3 指针
 * @return 成功返回 true
 * @details 创建 users/messages/offline_messages/file_transfers/verify_codes 表
 */
bool SQLiteMgr::CreateTables(sqlite3 *db)
{
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            uid INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT DEFAULT '',
            avatar_path TEXT DEFAULT '',
            created_at INTEGER NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            status INTEGER DEFAULT 0,
            UNIQUE(from_uid, to_uid, timestamp)
        );
        
        CREATE INDEX IF NOT EXISTS idx_messages_search 
            ON messages(from_uid, to_uid, timestamp DESC);
        
        CREATE INDEX IF NOT EXISTS idx_messages_keyword 
            ON messages(from_uid, to_uid);
        
        CREATE TABLE IF NOT EXISTS offline_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            status INTEGER DEFAULT 0
        );
        
        CREATE TABLE IF NOT EXISTS file_transfers (
            task_id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            filename TEXT NOT NULL,
            total_size INTEGER NOT NULL,
            transferred_size INTEGER DEFAULT 0,
            status INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS verify_codes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL,
            code TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        );
    )";

    char *err_msg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return false;
    }

    const char *migration_sql = "ALTER TABLE users ADD COLUMN email TEXT DEFAULT ''";
    sqlite3_exec(db, migration_sql, nullptr, nullptr, nullptr);

    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return false;
    }

    return true;
}

/**
 * @brief 保存聊天消息
 * @param msg 消息结构体
 * @return 成功返回 true
 */
bool SQLiteMgr::SaveMessage(const ChatMessage &msg)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "INSERT INTO messages (from_uid, to_uid, content, timestamp, status) VALUES (?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

/**
 * @brief 获取两个用户之间的消息历史
 * @param uid1 用户1 ID
 * @param uid2 用户2 ID
 * @param before_time 时间上限（毫秒时间戳）
 * @param limit 最大返回条数
 * @return 消息列表，按时间倒序
 */
std::vector<ChatMessage> SQLiteMgr::GetMessages(int uid1, int uid2, int64_t before_time, int limit)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return {};
    }
    sqlite3 *db = guard.Get();

    std::vector<ChatMessage> messages;
    ScopedStmt stmt(db, R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM messages 
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND timestamp < ?
        ORDER BY timestamp DESC 
        LIMIT ?
    )");
    if (!stmt)
    {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, uid1);
    sqlite3_bind_int(stmt, 2, uid2);
    sqlite3_bind_int(stmt, 3, uid2);
    sqlite3_bind_int(stmt, 4, uid1);
    sqlite3_bind_int64(stmt, 5, before_time);
    sqlite3_bind_int(stmt, 6, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        messages.push_back(msg);
    }

    return messages;
}

/**
 * @brief 注册新用户
 * @param username 用户名
 * @param password_hash 密码哈希（客户端已处理）
 * @param email 邮箱
 * @return 认证结果，包含 uid/token 或错误码
 * @details 采用 salt+$+salted_hash 格式存储密码
 */
AuthResult SQLiteMgr::RegisterUser(
    const std::string &username, const std::string &password_hash, const std::string &email)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        AuthResult r;
        r.error = 1;
        return r;
    }
    sqlite3 *db = guard.Get();

    auto existing = GetUserByUsername_unlocked(db, username);
    if (existing.has_value())
    {
        AuthResult r;
        r.error = 1005;
        return r;
    }

    std::string salt = GenerateSalt();
    std::string salted_hash = SHA256(password_hash + salt);
    std::string stored_password = salt + "$" + salted_hash;

    ScopedStmt stmt(
        db, "INSERT INTO users (username, password_hash, email, avatar_path, created_at) VALUES (?, ?, ?, '', ?)");
    if (!stmt)
    {
        AuthResult r;
        r.error = 1;
        return r;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, stored_password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, time(nullptr));

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        AuthResult r;
        r.error = 1;
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

/**
 * @brief 用户登录验证
 * @param username 用户名
 * @param password_hash 密码哈希
 * @return 认证结果，包含 uid/token 或错误码
 */
AuthResult SQLiteMgr::LoginUser(const std::string &username, const std::string &password_hash)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        AuthResult r;
        r.error = 1;
        return r;
    }
    sqlite3 *db = guard.Get();

    auto user = GetUserByUsername_unlocked(db, username);
    if (!user.has_value())
    {
        AuthResult r;
        r.error = 1007;
        return r;
    }

    std::string stored = user->password_hash;
    auto dollar_pos = stored.find('$');
    if (dollar_pos == std::string::npos)
    {
        if (stored != password_hash)
        {
            AuthResult r;
            r.error = 1006;
            return r;
        }
    }
    else
    {
        std::string salt = stored.substr(0, dollar_pos);
        std::string expected_hash = SHA256(password_hash + salt);
        std::string stored_hash = stored.substr(dollar_pos + 1);
        if (expected_hash != stored_hash)
        {
            AuthResult r;
            r.error = 1006;
            return r;
        }
    }

    AuthResult r;
    r.error = 0;
    r.uid = user->uid;
    r.token = SecureRandomHex(32);
    r.username = user->username;
    return r;
}

/**
 * @brief 发送验证码到邮箱
 * @param email 邮箱地址
 * @return 发送成功返回 true
 * @details 生成的 6 位验证码有效期 10 分钟
 */
bool SQLiteMgr::SendVerifyCode(const std::string &email, int &out_code)
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

    spdlog::info("[SQLiteMgr] Generated verify code for {}: {}", email, code);
    out_code = code;

    ScopedStmt del_stmt(db, "DELETE FROM verify_codes WHERE email = ?");
    if (del_stmt)
    {
        sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(del_stmt);
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
    sqlite3_bind_int64(ins_stmt, 4, now + 600);

    spdlog::info("[Auth] VerifyCode for {}: {} (DEV: hardcoded)", email, code);

    return sqlite3_step(ins_stmt) == SQLITE_DONE;
}

/**
 * @brief 校验验证码
 * @param email 邮箱地址
 * @param code 验证码
 * @return 0 成功，1003 过期/无效，1004 不存在
 */
int SQLiteMgr::CheckVerifyCode(const std::string &email, const std::string &code)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return 1003;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "SELECT expires_at FROM verify_codes WHERE email = ? AND code = ? ORDER BY id DESC LIMIT 1");
    if (!stmt)
    {
        return 1003;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int64_t expires_at = sqlite3_column_int64(stmt, 0);

        if (time(nullptr) > expires_at)
        {
            return 1003;
        }
        return 0;
    }

    return 1004;
}

/**
 * @brief 重置密码
 * @param username 用户名
 * @param email 邮箱
 * @param code 验证码
 * @param new_password_hash 新密码哈希
 * @return 成功返回 true
 */
bool SQLiteMgr::ResetPassword(
    const std::string &username, const std::string &email, const std::string &code,
    const std::string &new_password_hash)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    auto user = GetUserByUsername_unlocked(db, username);
    if (!user.has_value())
    {
        return false;
    }

    if (user->email != email)
    {
        return false;
    }

    int verify_result = CheckVerifyCode_unlocked(db, email, code);
    if (verify_result != 0)
    {
        return false;
    }

    ScopedStmt stmt(db, "UPDATE users SET password_hash = ? WHERE uid = ?");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, new_password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user->uid);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (success)
    {
        ScopedStmt del_stmt(db, "DELETE FROM verify_codes WHERE email = ?");
        if (del_stmt)
        {
            sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del_stmt);
        }
    }

    return success;
}

/**
 * @brief 搜索消息
 * @param uid1 用户1 ID
 * @param uid2 用户2 ID
 * @param keyword 关键词（LIKE 模糊匹配）
 * @param limit 最大返回条数
 * @return 匹配的消息列表
 */
std::vector<ChatMessage> SQLiteMgr::SearchMessages(int uid1, int uid2, const std::string &keyword, int limit)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return {};
    }
    sqlite3 *db = guard.Get();

    std::vector<ChatMessage> messages;
    ScopedStmt stmt(db, R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM messages 
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND content LIKE ?
        ORDER BY timestamp DESC 
        LIMIT ?
    )");
    if (!stmt)
    {
        return messages;
    }

    std::string pattern = "%" + keyword + "%";

    sqlite3_bind_int(stmt, 1, uid1);
    sqlite3_bind_int(stmt, 2, uid2);
    sqlite3_bind_int(stmt, 3, uid2);
    sqlite3_bind_int(stmt, 4, uid1);
    sqlite3_bind_text(stmt, 5, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        messages.push_back(msg);
    }

    return messages;
}

bool SQLiteMgr::SaveUser(const User &user)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(
        db, "INSERT INTO users (username, password_hash, email, avatar_path, created_at) VALUES (?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user.password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user.avatar_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, user.created_at);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::optional<User> SQLiteMgr::GetUserByUsername(const std::string &username)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return std::nullopt;
    }
    sqlite3 *db = guard.Get();

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
        user.username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        user.password_hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        user.email = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        user.avatar_path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        user.created_at = sqlite3_column_int64(stmt, 5);
        return user;
    }

    return std::nullopt;
}

std::optional<User> SQLiteMgr::GetUserByUsername_unlocked(sqlite3 *db, const std::string &username)
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
        user.username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        user.password_hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        user.email = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        user.avatar_path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        user.created_at = sqlite3_column_int64(stmt, 5);
        return user;
    }
    return std::nullopt;
}

int SQLiteMgr::CheckVerifyCode_unlocked(sqlite3 *db, const std::string &email, const std::string &code)
{
    ScopedStmt stmt(db, "SELECT expires_at FROM verify_codes WHERE email = ? AND code = ? ORDER BY id DESC LIMIT 1");
    if (!stmt)
    {
        return 1003;
    }
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int64_t expires_at = sqlite3_column_int64(stmt, 0);
        if (time(nullptr) > expires_at)
        {
            return 1003;
        }
        return 0;
    }
    return 1004;
}

std::optional<User> SQLiteMgr::GetUserByUid(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return std::nullopt;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "SELECT uid, username, password_hash, email, avatar_path, created_at FROM users WHERE uid = ?");
    if (!stmt)
    {
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, uid);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        User user;
        user.uid = sqlite3_column_int(stmt, 0);
        user.username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        user.password_hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        user.email = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        user.avatar_path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        user.created_at = sqlite3_column_int64(stmt, 5);
        return user;
    }

    return std::nullopt;
}

bool SQLiteMgr::UpdateUserAvatar(int uid, const std::string &avatar_path)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "UPDATE users SET avatar_path = ? WHERE uid = ?");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, avatar_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

/**
 * @brief 保存离线消息
 * @param msg 消息结构体
 * @return 成功返回 true
 */
bool SQLiteMgr::SaveOfflineMessage(const ChatMessage &msg)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(
        db, "INSERT INTO offline_messages (from_uid, to_uid, content, timestamp, status) VALUES (?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

/**
 * @brief 获取用户离线消息
 * @param uid 用户 ID
 * @return 消息列表，按时间升序
 */
std::vector<ChatMessage> SQLiteMgr::GetOfflineMessages(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return {};
    }
    sqlite3 *db = guard.Get();

    std::vector<ChatMessage> messages;
    ScopedStmt stmt(db, R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM offline_messages 
        WHERE to_uid = ?
        ORDER BY timestamp ASC
    )");
    if (!stmt)
    {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, uid);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        messages.push_back(msg);
    }

    return messages;
}

std::vector<ChatMessage> SQLiteMgr::GetOfflineMessages(int uid, int limit, int64_t after_id)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return {};
    }
    sqlite3 *db = guard.Get();

    std::vector<ChatMessage> messages;
    ScopedStmt stmt(db, R"(
        SELECT id, from_uid, to_uid, content, timestamp, status
        FROM offline_messages
        WHERE to_uid = ? AND id > ?
        ORDER BY id ASC
        LIMIT ?
    )");
    if (!stmt)
    {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int64(stmt, 2, after_id);
    sqlite3_bind_int(stmt, 3, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        messages.push_back(msg);
    }

    return messages;
}

/**
 * @brief 获取离线消息数量
 * @param uid 用户 ID
 * @return 消息条数
 */
int64_t SQLiteMgr::GetOfflineMessageCount(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return 0;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "SELECT COUNT(*) FROM offline_messages WHERE to_uid = ?");
    if (!stmt)
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, uid);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        return sqlite3_column_int64(stmt, 0);
    }

    return 0;
}

/**
 * @brief 清空用户离线消息
 * @param uid 用户 ID
 * @return 成功返回 true
 */
bool SQLiteMgr::ClearOfflineMessages(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "DELETE FROM offline_messages WHERE to_uid = ?");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

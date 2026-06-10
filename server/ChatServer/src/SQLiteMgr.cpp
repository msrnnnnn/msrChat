/**
 * @file SQLiteMgr.cpp
 * @brief SQLite 数据库管理实现
 * @details 包含连接池、用户认证、消息存储、验证码管理。
 */
#include "SQLiteMgr.h"
#include "const.h"
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
    // 将 256-bit 哈希转换为 64 字符十六进制字符串
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
    // OpenSSL RAND_bytes 优先，失败时回退到 random_device
    if (RAND_bytes(buf.data(), bytes) != 1)
    {
        std::random_device rd;
        for (int i = 0; i < bytes; ++i) buf[i] = static_cast<unsigned char>(rd());
    }
    // 转换为大写十六进制字符串
    std::stringstream ss;
    for (int i = 0; i < bytes; ++i)
    {
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(buf[i]);
    }
    return ss.str();
}

/**
 * @brief 生成 16 字节随机盐值
 * @details 16 字节 × 2 字符/字节 = 32 字符十六进制字符串
 */
static std::string GenerateSalt()
{
    return SecureRandomHex(16);
}

/**
 * @brief 从 SQLite 结果集读取 Phase 7 新增的 6 列
 * @details 列索引 6-11: type, image_id, recalled, recalled_at, edited, edited_at
 */
static void ReadPhase7Columns(sqlite3_stmt *stmt, ChatMessage &msg)
{
    msg.type = sqlite3_column_int(stmt, 6);
    const unsigned char *iid = sqlite3_column_text(stmt, 7);
    if (iid) msg.image_id = std::string(reinterpret_cast<const char *>(iid));
    msg.recalled = sqlite3_column_int(stmt, 8) != 0;
    msg.recalled_at = sqlite3_column_int64(stmt, 9);
    msg.edited = sqlite3_column_int(stmt, 10) != 0;
    msg.edited_at = sqlite3_column_int64(stmt, 11);
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

    sqlite3_busy_timeout(*db, 5000);  // 5 second busy timeout for concurrent access

    return true;
}

/**
 * @brief 从连接池获取一个连接
 * @return 可用连接，不可用时返回 nullptr
 * @details 条件变量阻塞等待，最多 30 秒；池关闭返回 nullptr
 */
std::shared_ptr<SQLiteConnection> SQLiteConnectionPool::Acquire()
{
    std::unique_lock<std::mutex> lock(_mutex);

    // 等待可用连接或池关闭，超时 30 秒
    if (!_cv.wait_for(lock, std::chrono::seconds(30),
                      [this] { return !_available_connections.empty() || _shutdown.load(); }))
    {
        spdlog::error("[SQLiteConnectionPool] Acquire timed out after 30s");
        return nullptr;
    }

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
 * @details 标记未使用后放回可用队列，通知一个等待线程
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

    // 批量创建连接并加入可用队列与全连接列表
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
            client_msg_id TEXT DEFAULT '',
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
            status INTEGER DEFAULT 0,
            client_msg_id TEXT DEFAULT '',
            type INTEGER DEFAULT 0,
            image_id TEXT DEFAULT ''
        );
        
        CREATE TABLE IF NOT EXISTS verify_codes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL,
            code TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS tokens (
            uid INTEGER PRIMARY KEY,
            token TEXT NOT NULL,
            created_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS recall_notify_queue (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uid INTEGER NOT NULL,
            msg_timestamp INTEGER NOT NULL,
            recall_uid INTEGER NOT NULL,
            recall_ts INTEGER NOT NULL,
            recalled_to INTEGER NOT NULL
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

    // Phase 2 — 向后兼容迁移：users 表增加 email 列（ignore duplicate column）
    const char *migration_sql = "ALTER TABLE users ADD COLUMN email TEXT DEFAULT ''";
    char *mig_err = nullptr;
    int mig_rc = sqlite3_exec(db, migration_sql, nullptr, nullptr, &mig_err);
    if (mig_rc != SQLITE_OK && mig_err)
    {
        std::string err_str(mig_err);
        sqlite3_free(mig_err);
        if (err_str.find("duplicate column") == std::string::npos)
        {
            spdlog::warn("[SQLiteMgr] Migration ALTER TABLE users failed: {}", err_str);
        }
    }

    const char *offline_migration_sql = "ALTER TABLE offline_messages ADD COLUMN client_msg_id TEXT DEFAULT ''";
    mig_rc = sqlite3_exec(db, offline_migration_sql, nullptr, nullptr, &mig_err);
    if (mig_rc != SQLITE_OK && mig_err)
    {
        std::string err_str(mig_err);
        sqlite3_free(mig_err);
        if (err_str.find("duplicate column") == std::string::npos)
        {
            spdlog::warn("[SQLiteMgr] Migration ALTER TABLE offline_messages failed: {}", err_str);
        }
    }

    // === Phase 7 — messages 表增 6 列（recall/edit + type/image_id）===
    const char *p7_migrations[] = {
        "ALTER TABLE messages ADD COLUMN type INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN image_id TEXT DEFAULT ''",
        "ALTER TABLE messages ADD COLUMN recalled INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN recalled_at INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN edited INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN edited_at INTEGER DEFAULT 0"
    };
    for (const char *p7_sql : p7_migrations)
    {
        char *p7_err = nullptr;
        int p7_rc = sqlite3_exec(db, p7_sql, nullptr, nullptr, &p7_err);
        if (p7_rc != SQLITE_OK && p7_err)
        {
            std::string p7_err_str(p7_err);
            sqlite3_free(p7_err);
            if (p7_err_str.find("duplicate column") == std::string::npos)
            {
                spdlog::warn("[SQLiteMgr] P7 migration failed: {} (err={})", p7_sql, p7_err_str);
            }
            // duplicate column 视为成功（旧库已有）
        }
    }

    // === Phase 2 — messages 表增 client_msg_id 列 + 去重索引 ===
    {
        const char *c2_sql = "ALTER TABLE messages ADD COLUMN client_msg_id TEXT DEFAULT ''";
        char *c2_err = nullptr;
        int c2_rc = sqlite3_exec(db, c2_sql, nullptr, nullptr, &c2_err);
        if (c2_rc != SQLITE_OK && c2_err)
        {
            std::string c2_err_str(c2_err);
            sqlite3_free(c2_err);
            if (c2_err_str.find("duplicate column") == std::string::npos)
            {
                spdlog::warn("[SQLiteMgr] Phase 2 migration failed: {} (err={})", c2_sql, c2_err_str);
            }
        }
    }
    {
        const char *idx_sql =
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_client_msg_id "
            "ON messages(client_msg_id) WHERE client_msg_id != ''";
        char *idx_err = nullptr;
        sqlite3_exec(db, idx_sql, nullptr, nullptr, &idx_err);
        if (idx_err)
        {
            spdlog::warn("[SQLiteMgr] Phase 2 index creation failed: {}", idx_err);
            sqlite3_free(idx_err);
        }
    }

    // === Phase D — offline_messages 表增 type + image_id 列 ===
    const char *pd_migrations[] = {
        "ALTER TABLE offline_messages ADD COLUMN type INTEGER DEFAULT 0",
        "ALTER TABLE offline_messages ADD COLUMN image_id TEXT DEFAULT ''"
    };
    for (const char *pd_sql : pd_migrations)
    {
        char *pd_err = nullptr;
        int pd_rc = sqlite3_exec(db, pd_sql, nullptr, nullptr, &pd_err);
        if (pd_rc != SQLITE_OK && pd_err)
        {
            std::string pd_err_str(pd_err);
            sqlite3_free(pd_err);
            if (pd_err_str.find("duplicate column") == std::string::npos)
            {
                spdlog::warn("[SQLiteMgr] PD migration failed: {} (err={})", pd_sql, pd_err_str);
            }
        }
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

    // Phase 7: 增 6 列 (type/image_id/recalled/recalled_at/edited/edited_at)
    ScopedStmt stmt(db,
        "INSERT INTO messages (from_uid, to_uid, content, timestamp, status, "
        "client_msg_id, type, image_id, recalled, recalled_at, edited, edited_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);
    sqlite3_bind_text(stmt, 6, msg.client_msg_id.c_str(), -1, SQLITE_TRANSIENT);
    // Phase 7 — 6 个新字段
    sqlite3_bind_int(stmt, 7, msg.type);
    sqlite3_bind_text(stmt, 8, msg.image_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, msg.recalled ? 1 : 0);
    sqlite3_bind_int64(stmt, 10, msg.recalled_at);
    sqlite3_bind_int(stmt, 11, msg.edited ? 1 : 0);
    sqlite3_bind_int64(stmt, 12, msg.edited_at);

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
    // Phase 7: 增 6 列 (type/image_id/recalled/recalled_at/edited/edited_at)
    ScopedStmt stmt(db, R"(
        SELECT id, from_uid, to_uid, content, timestamp, status,
               type, image_id, recalled, recalled_at, edited, edited_at
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
        msg.content = SafeColumnText(stmt, 3);
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        ReadPhase7Columns(stmt, msg);
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

    // 生成盐值，格式: salt + "$" + SHA256(password_hash + salt)
    std::string salt = GenerateSalt();
    std::string salted_hash = SHA256(password_hash + salt);
    std::string stored_password = salt + "$" + salted_hash;

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

    // 解析存储的密码：salt$salted_hash 格式 → 重新计算验证
    std::string stored = user->password_hash;
    auto dollar_pos = stored.find('$');
    if (dollar_pos == std::string::npos)
    {
        // 旧版明文密码，直接比对
        if (stored != password_hash)
        {
            AuthResult r;
            r.error = ERR_PASSWD_ERR;
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
            r.error = ERR_PASSWD_ERR;
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

    spdlog::debug("[SQLiteMgr] Generated verify code for {}: {}", email, code);
    out_code = code;

    ScopedStmt del_stmt(db, "DELETE FROM verify_codes WHERE email = ?");
    if (del_stmt)
    {
        sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(del_stmt) != SQLITE_DONE)
        {
            spdlog::warn("[SQLiteMgr] Failed to delete old verify codes for {}", email);
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

/**
 * @brief 重置密码
 * @param username 用户名
 * @param email 邮箱
 * @param code 验证码
 * @param new_password_hash 新密码哈希
 * @return 成功返回 true
 */
int SQLiteMgr::ResetPassword(
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
    std::string salted_hash = SHA256(new_password_hash + salt);
    std::string stored_password = salt + "$" + salted_hash;

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

std::optional<User> SQLiteMgr::GetUserByUsername(const std::string &username)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
        return std::nullopt;
    return GetUserByUsernameUnlocked(guard.Get(), username);
}

std::optional<User> SQLiteMgr::GetUserByUsernameUnlocked(sqlite3 *db, const std::string &username)
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
        db, "INSERT INTO offline_messages (from_uid, to_uid, content, timestamp, status, client_msg_id, type, image_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    if (msg.type == 1)
    {
        // 图片消息：content 存 Base64 编码的 protobuf binary，用 blob 绑定保留 \0 字节
        sqlite3_bind_blob(stmt, 3, msg.content.data(), static_cast<int>(msg.content.size()), SQLITE_TRANSIENT);
    }
    else
    {
        // 文本消息：普通 bind_text
        sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);
    sqlite3_bind_text(stmt, 6, msg.client_msg_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, msg.type);
    sqlite3_bind_text(stmt, 8, msg.image_id.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt) == SQLITE_DONE;
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
        SELECT om.id, om.from_uid, om.to_uid, om.content, om.timestamp, om.status, om.client_msg_id, om.type, om.image_id
        FROM offline_messages om
        LEFT JOIN messages m ON m.timestamp = om.timestamp AND m.from_uid = om.from_uid
        WHERE om.to_uid = ? AND om.id > ? AND (m.recalled IS NULL OR m.recalled = 0)
        ORDER BY om.id ASC
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
        // content: type=1（图片）时用 blob API 读取（保留 \0 字节），type=0（文本）用 text API
        msg.type = sqlite3_column_int(stmt, 7);
        if (msg.type == 1)
        {
            const void *blob = sqlite3_column_blob(stmt, 3);
            int blob_size = sqlite3_column_bytes(stmt, 3);
            if (blob && blob_size > 0)
            {
                msg.content = std::string(static_cast<const char *>(blob), blob_size);
            }
        }
        else
        {
            const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            msg.content = text ? std::string(text) : "";
        }
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        const char *client_msg_id_text =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        msg.client_msg_id = client_msg_id_text ? std::string(client_msg_id_text) : "";
        const char *image_id_text =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        msg.image_id = image_id_text ? std::string(image_id_text) : "";
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

    ScopedStmt stmt(db, R"(
        SELECT COUNT(*) FROM offline_messages om
        LEFT JOIN messages m ON m.timestamp = om.timestamp AND m.from_uid = om.from_uid
        WHERE om.to_uid = ? AND (m.recalled IS NULL OR m.recalled = 0)
    )");
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

/**
 * @brief 持久化 Token 到 tokens 表
 * @param uid 用户 ID
 * @param token Token 字符串
 * @return 写入成功返回 true
 * @details INSERT OR REPLACE：同 uid 覆盖旧 Token
 */
bool SQLiteMgr::SaveToken(int uid, const std::string &token)
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

/**
 * @brief 从 tokens 表按 UID 获取 Token
 * @param uid 用户 ID
 * @return Token 字符串，不存在则 nullopt
 */
std::optional<std::string> SQLiteMgr::GetTokenFromDB(int uid)
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

/**
 * @brief 获取 tokens 表中所有 (uid, token) 对
 * @return 键值对列表，用于服务启动时从数据库恢复 TokenManager 缓存
 */
std::vector<std::pair<int, std::string>> SQLiteMgr::GetAllTokens()
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return {};
    sqlite3 *db = guard.Get();
    std::vector<std::pair<int, std::string>> tokens;
    ScopedStmt stmt(db, "SELECT uid, token FROM tokens");
    if (!stmt) return tokens;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int uid = sqlite3_column_int(stmt, 0);
        std::string token = SafeColumnText(stmt, 1);
        tokens.emplace_back(uid, token);
    }
    return tokens;
}

// === Phase 7 — 撤回 / 编辑 ===

/**
 * @brief 按 timestamp + from_uid 精确查找消息
 * @details 用于 HandleChatRecall / HandleChatEdit 的存在性 + 所有权校验
 */
std::optional<ChatMessage> SQLiteMgr::GetMessageByTimestamp(int64_t timestamp, int from_uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return std::nullopt;
    sqlite3 *db = guard.Get();

    const char *sql = R"SQL(
        SELECT id, from_uid, to_uid, content, timestamp, status,
               type, image_id, recalled, recalled_at, edited, edited_at
        FROM messages
        WHERE timestamp = ? AND from_uid = ?
        LIMIT 1
    )SQL";

    ScopedStmt stmt(db, sql);
    if (!stmt) return std::nullopt;
    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_int(stmt, 2, from_uid);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = SafeColumnText(stmt, 3);
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        msg.type = sqlite3_column_int(stmt, 6);
        const unsigned char *iid = sqlite3_column_text(stmt, 7);
        if (iid) msg.image_id = std::string(reinterpret_cast<const char *>(iid));
        msg.recalled = sqlite3_column_int(stmt, 8) != 0;
        msg.recalled_at = sqlite3_column_int64(stmt, 9);
        msg.edited = sqlite3_column_int(stmt, 10) != 0;
        msg.edited_at = sqlite3_column_int64(stmt, 11);
        return msg;
    }
    return std::nullopt;
}

/**
 * @brief 标记消息为已撤回（Phase 7）
 * @param timestamp 消息时间戳（毫秒）
 * @param from_uid 消息发送方
 * @param recall_ts 撤回时间戳（毫秒）
 * @return UPDATE 成功
 */
bool SQLiteMgr::MarkMessageRecalled(int64_t timestamp, int from_uid, int64_t recall_ts)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "UPDATE messages SET recalled = 1, recalled_at = ? WHERE timestamp = ? AND from_uid = ?");
    if (!stmt) return false;
    sqlite3_bind_int64(stmt, 1, recall_ts);
    sqlite3_bind_int64(stmt, 2, timestamp);
    sqlite3_bind_int(stmt, 3, from_uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

/**
 * @brief 更新消息内容（编辑，Phase 7）
 */
bool SQLiteMgr::UpdateMessageContent(int64_t timestamp, int from_uid,
                                      const std::string &new_content, int64_t edit_ts)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "UPDATE messages SET content = ?, edited = 1, edited_at = ? "
        "WHERE timestamp = ? AND from_uid = ?");
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, new_content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, edit_ts);
    sqlite3_bind_int64(stmt, 3, timestamp);
    sqlite3_bind_int(stmt, 4, from_uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

// === Phase 2 — Recall Notify 队列 ===

bool SQLiteMgr::EnqueueRecallNotify(int uid, int64_t msg_timestamp, int recall_uid,
                                     int64_t recall_ts, int recalled_to)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "INSERT INTO recall_notify_queue (uid, msg_timestamp, recall_uid, recall_ts, recalled_to) "
        "VALUES (?, ?, ?, ?, ?)");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int64(stmt, 2, msg_timestamp);
    sqlite3_bind_int(stmt, 3, recall_uid);
    sqlite3_bind_int64(stmt, 4, recall_ts);
    sqlite3_bind_int(stmt, 5, recalled_to);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::vector<RecallNotifyEntry> SQLiteMgr::PopRecallNotifies(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return {};
    sqlite3 *db = guard.Get();

    std::vector<RecallNotifyEntry> entries;
    ScopedStmt stmt(db,
        "SELECT id, uid, msg_timestamp, recall_uid, recall_ts, recalled_to "
        "FROM recall_notify_queue WHERE uid = ? "
        "ORDER BY id ASC");
    if (!stmt) return entries;
    sqlite3_bind_int(stmt, 1, uid);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        RecallNotifyEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.uid = sqlite3_column_int(stmt, 1);
        e.msg_timestamp = sqlite3_column_int64(stmt, 2);
        e.recall_uid = sqlite3_column_int(stmt, 3);
        e.recall_ts = sqlite3_column_int64(stmt, 4);
        e.recalled_to = sqlite3_column_int(stmt, 5);
        entries.push_back(e);
    }
    return entries;
}

bool SQLiteMgr::ClearRecallNotifies(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "DELETE FROM recall_notify_queue WHERE uid = ?");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

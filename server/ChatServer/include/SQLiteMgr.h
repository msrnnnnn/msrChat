#pragma once
/**
 * @file SQLiteMgr.h
 * @brief SQLite 数据库管理层 —— 连接池、消息持久化、用户认证
 * @details 包含 SQLiteConnection 连接封装、SQLiteConnectionPool 连接池、
 *          SQLiteConnectionGuard RAII 连接守卫、ScopedStmt 语句生命周期管理，
 *          以及顶层的 SQLiteMgr 业务操作接口。
 *          所有数据库操作通过连接池获取连接，支持多线程并发访问。
 */
#ifndef SQLITE_MGR_H
#define SQLITE_MGR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief 撤回通知记录 —— 通知目标用户某条消息已被撤回
 */
struct RecallNotifyEntry
{
    int64_t id = 0;
    int     uid          = 0;
    int64_t msg_timestamp = 0;
    int     recall_uid   = 0;
    int64_t recall_ts    = 0;
    int     recalled_to  = 0;
};

/**
 * @brief 聊天消息数据库记录
 */
struct ChatMessage
{
    int64_t id = 0;
    int from_uid = 0;
    int to_uid = 0;
    std::string content;
    int64_t timestamp = 0;
    int status = 0;
    std::string client_msg_id;
    // === Phase 7 新增 ===
    int     type        = 0;     ///< 0=text, 1=image
    std::string image_id;        ///< UUID（type=1 时）
    bool    recalled    = false;
    int64_t recalled_at = 0;
    bool    edited      = false;
    int64_t edited_at   = 0;
};

/**
 * @brief 用户数据库记录
 */
struct User
{
    int uid;
    std::string username;
    std::string password_hash;
    std::string email;
    std::string avatar_path;
    int64_t created_at;
};

/**
 * @brief 认证操作结果
 */
struct AuthResult
{
    int error = 0;
    int uid = 0;
    std::string token;
    std::string username;
};

/**
 * @brief SQLite 数据库连接封装 —— 线程内绑定，记录使用状态
 */
class SQLiteConnection
{
public:
    explicit SQLiteConnection(sqlite3 *db) : _db(db), _in_use(false) {}
    sqlite3 *Get() const { return _db; }
    bool IsInUse() const { return _in_use; }
    void SetInUse(bool in_use) { _in_use = in_use; }
    void Reset()
    {
    }

private:
    sqlite3 *_db;
    bool _in_use;
};

/**
 * @brief SQLite 连接池 —— 管理多个 SQLiteConnection，支持阻塞式获取/归还
 * @details 使用 std::queue 管理空闲连接，条件变量实现等待通知。
 *          Acquire() 在无可用连接时阻塞等待，Release() 归还后唤醒等待者。
 *          Shutdown() 关闭所有连接并唤醒所有等待线程。
 */
class SQLiteConnectionPool
{
    friend class SQLiteMgr;

public:
    explicit SQLiteConnectionPool(const std::string &db_path, int pool_size = 8);
    ~SQLiteConnectionPool();

    std::shared_ptr<SQLiteConnection> Acquire();
    void Release(std::shared_ptr<SQLiteConnection> conn);
    void Shutdown();

    bool IsInitialized() const { return _initialized.load(); }

private:
    bool InitializeConnection(sqlite3 **db);
    void CloseAllConnections();

    std::string _db_path;
    int _pool_size;
    std::queue<std::shared_ptr<SQLiteConnection>> _available_connections;
    std::vector<std::shared_ptr<SQLiteConnection>> _all_connections;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _shutdown{false};
    std::atomic<bool> _initialized{false};
};

/**
 * @brief SQLite 连接 RAII 守卫 —— 构造时获取连接，析构时自动归还
 * @details 提供 Get() 获取原始 sqlite3* 指针，支持布尔判断连接是否有效。
 *          移动语义：移动后源对象释放所有权，避免重复归还。
 */
class SQLiteConnectionGuard
{
public:
    explicit SQLiteConnectionGuard(std::shared_ptr<SQLiteConnectionPool> pool)
        : _pool(pool), _conn(nullptr)
    {
        if (_pool)
        {
            _conn = _pool->Acquire();
        }
    }

    ~SQLiteConnectionGuard()
    {
        if (_conn && _pool)
        {
            _conn->Reset();
            _pool->Release(_conn);
        }
    }

    SQLiteConnectionGuard(const SQLiteConnectionGuard &) = delete;
    SQLiteConnectionGuard &operator=(const SQLiteConnectionGuard &) = delete;

    SQLiteConnectionGuard(SQLiteConnectionGuard &&other) noexcept
        : _pool(std::move(other._pool)), _conn(std::move(other._conn))
    {
        other._conn = nullptr;
    }

    SQLiteConnectionGuard &operator=(SQLiteConnectionGuard &&other) noexcept
    {
        if (this != &other)
        {
            if (_conn && _pool)
            {
                _conn->Reset();
                _pool->Release(_conn);
            }
            _pool = std::move(other._pool);
            _conn = std::move(other._conn);
            other._conn = nullptr;
        }
        return *this;
    }

    sqlite3 *Get() const
    {
        return _conn ? _conn->Get() : nullptr;
    }

    explicit operator bool() const
    {
        return _conn != nullptr && _conn->Get() != nullptr;
    }

private:
    std::shared_ptr<SQLiteConnectionPool> _pool;
    std::shared_ptr<SQLiteConnection> _conn;
};

/**
 * @brief SQLite Statement RAII 封装 —— 构造时 prepare，析构时自动 finalize
 * @details 支持移动语义，提供 operator-> 和隐式转换 sqlite3_stmt* 方便绑定参数。
 */
class ScopedStmt
{
public:
    ScopedStmt() : _stmt(nullptr), _db(nullptr) {}

    ScopedStmt(sqlite3 *db, const char *sql) : _stmt(nullptr), _db(db)
    {
        if (_db)
        {
            int rc = sqlite3_prepare_v2(_db, sql, -1, &_stmt, nullptr);
            if (rc != SQLITE_OK)
            {
                fprintf(stderr, "[ScopedStmt] prepare failed: %s\n", sqlite3_errmsg(_db));
            }
        }
    }

    ~ScopedStmt()
    {
        if (_stmt)
        {
            sqlite3_finalize(_stmt);
            _stmt = nullptr;
        }
    }

    ScopedStmt(const ScopedStmt &) = delete;
    ScopedStmt &operator=(const ScopedStmt &) = delete;

    ScopedStmt(ScopedStmt &&other) noexcept : _stmt(other._stmt), _db(other._db)
    {
        other._stmt = nullptr;
        other._db = nullptr;
    }

    ScopedStmt &operator=(ScopedStmt &&other) noexcept
    {
        if (this != &other)
        {
            if (_stmt)
            {
                sqlite3_finalize(_stmt);
            }
            _stmt = other._stmt;
            _db = other._db;
            other._stmt = nullptr;
            other._db = nullptr;
        }
        return *this;
    }

    bool isValid() const { return _stmt != nullptr; }
    sqlite3_stmt *get() const { return _stmt; }
    sqlite3_stmt *operator->() const { return _stmt; }
    operator sqlite3_stmt *() const { return _stmt; }
    explicit operator bool() const { return isValid(); }

private:
    sqlite3_stmt *_stmt;
    sqlite3 *_db;
};

/**
 * @brief SQLite 业务管理器（单例） —— 封装所有数据库业务操作
 * @details 提供消息 CRUD、用户注册/登录、验证码、Token 持久化等接口。
 *          内部通过 SQLiteConnectionPool 管理连接，每个公开方法内部创建
 *          SQLiteConnectionGuard 自动获取和归还连接。
 */
class SQLiteMgr
{
public:
    static SQLiteMgr &Instance();

    bool Init(const std::string &db_path, int pool_size = 8);
    std::shared_ptr<SQLiteConnectionPool> GetPool() const { return _pool; }
    void Shutdown();

    bool SaveMessage(const ChatMessage &msg);
    std::vector<ChatMessage> GetMessages(int uid1, int uid2, int64_t before_time, int limit = 50);

    // Phase 7 — 撤回 / 编辑
    bool MarkMessageRecalled(int64_t timestamp, int from_uid, int64_t recall_ts);
    bool UpdateMessageContent(int64_t timestamp, int from_uid, const std::string &new_content, int64_t edit_ts);
    std::optional<ChatMessage> GetMessageByTimestamp(int64_t timestamp, int from_uid);

    std::optional<User> GetUserByUsername(const std::string &username);

    // Phase 2 — Recall Notify
    bool EnqueueRecallNotify(int uid, int64_t msg_timestamp, int recall_uid, int64_t recall_ts, int recalled_to);
    std::vector<RecallNotifyEntry> PopRecallNotifies(int uid);
    bool ClearRecallNotifies(int uid);

    bool SaveOfflineMessage(const ChatMessage &msg);
    std::vector<ChatMessage> GetOfflineMessages(int uid, int limit, int64_t after_id = 0);
    int64_t GetOfflineMessageCount(int uid);
    bool ClearOfflineMessages(int uid);

    AuthResult RegisterUser(const std::string &username, const std::string &password_hash, const std::string &email);
    AuthResult LoginUser(const std::string &username, const std::string &password_hash);
    bool SendVerifyCode(const std::string &email, int &out_code);
    int CheckVerifyCode(const std::string &email, const std::string &code);
    int ResetPassword(
        const std::string &username, const std::string &email, const std::string &code,
        const std::string &new_password_hash);

    bool SaveToken(int uid, const std::string &token);
    std::optional<std::string> GetTokenFromDB(int uid);
    std::vector<std::pair<int, std::string>> GetAllTokens();

    SQLiteMgr(const SQLiteMgr &) = delete;
    SQLiteMgr &operator=(const SQLiteMgr &) = delete;

private:
    SQLiteMgr() = default;
    ~SQLiteMgr();

    bool CreateTables(sqlite3 *db);
    std::optional<User> GetUserByUsernameUnlocked(sqlite3 *db, const std::string &username);


    std::shared_ptr<SQLiteConnectionPool> _pool;
    std::atomic<bool> _initialized{false};
};

#endif

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

struct User
{
    int uid;
    std::string username;
    std::string password_hash;
    std::string email;
    std::string avatar_path;
    int64_t created_at;
};

struct AuthResult
{
    int error = 0;
    int uid = 0;
    std::string token;
    std::string username;
};

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

class SQLiteMgr
{
public:
    static SQLiteMgr &Instance();

    bool Init(const std::string &db_path, int pool_size = 8);
    std::shared_ptr<SQLiteConnectionPool> GetPool() const { return _pool; }
    void Shutdown();

    bool SaveMessage(const ChatMessage &msg);
    std::vector<ChatMessage> GetMessages(int uid1, int uid2, int64_t before_time, int limit = 50);
    std::vector<ChatMessage> SearchMessages(int uid1, int uid2, const std::string &keyword, int limit = 50);

    // Phase 7 — 撤回 / 编辑
    bool MarkMessageRecalled(int64_t timestamp, int from_uid, int64_t recall_ts);
    bool UpdateMessageContent(int64_t timestamp, int from_uid, const std::string &new_content, int64_t edit_ts);
    std::optional<ChatMessage> GetMessageByTimestamp(int64_t timestamp, int from_uid);

    bool SaveUser(const User &user);
    std::optional<User> GetUserByUsername(const std::string &username);

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
    bool RemoveTokenFromDB(int uid);
    std::optional<std::string> GetTokenFromDB(int uid);
    std::vector<std::pair<int, std::string>> GetAllTokens();

    SQLiteMgr(const SQLiteMgr &) = delete;
    SQLiteMgr &operator=(const SQLiteMgr &) = delete;

private:
    SQLiteMgr() = default;
    ~SQLiteMgr();

    bool CreateTables(sqlite3 *db);
    std::optional<User> GetUserByUsername_unlocked(sqlite3 *db, const std::string &username);
    int CheckVerifyCode_unlocked(sqlite3 *db, const std::string &email, const std::string &code);

    std::shared_ptr<SQLiteConnectionPool> _pool;
    std::atomic<bool> _initialized{false};
};

#endif

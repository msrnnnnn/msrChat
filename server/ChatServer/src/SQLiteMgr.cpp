#include "SQLiteMgr.h"
#include <cstring>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

SQLiteMgr::~SQLiteMgr()
{
    Shutdown();
}

SQLiteMgr &SQLiteMgr::Instance()
{
    static SQLiteMgr instance;
    return instance;
}

bool SQLiteMgr::Init(const std::string &db_path)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_initialized.load())
    {
        return true;
    }

    if (sqlite3_open(db_path.c_str(), &_db) != SQLITE_OK)
    {
        return false;
    }

    if (!CreateTables())
    {
        sqlite3_close(_db);
        _db = nullptr;
        return false;
    }

    _initialized.store(true);
    return true;
}

void SQLiteMgr::Shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_db)
    {
        sqlite3_close(_db);
        _db = nullptr;
    }

    _initialized.store(false);
}

bool SQLiteMgr::CreateTables()
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
    if (sqlite3_exec(_db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return false;
    }

    const char *migration_sql = "ALTER TABLE users ADD COLUMN email TEXT DEFAULT ''";
    sqlite3_exec(_db, migration_sql, nullptr, nullptr, nullptr);

    if (sqlite3_exec(_db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return false;
    }

    return true;
}

bool SQLiteMgr::SaveMessage(const ChatMessage &msg)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql = "INSERT INTO messages (from_uid, to_uid, content, timestamp, status) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

std::vector<ChatMessage> SQLiteMgr::GetMessages(int uid1, int uid2, int64_t before_time, int limit)
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<ChatMessage> messages;
    const char *sql = R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM messages 
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND timestamp < ?
        ORDER BY timestamp DESC 
        LIMIT ?
    )";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
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

    sqlite3_finalize(stmt);
    return messages;
}

AuthResult SQLiteMgr::RegisterUser(
    const std::string &username, const std::string &password_hash, const std::string &email)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto existing = GetUserByUsername_unlocked(username);
    if (existing.has_value())
    {
        AuthResult r;
        r.error = 1005;
        return r;
    }

    const char *sql =
        "INSERT INTO users (username, password_hash, email, avatar_path, created_at) VALUES (?, ?, ?, '', ?)";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        AuthResult r;
        r.error = 1;
        return r;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, time(nullptr));

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        AuthResult r;
        r.error = 1;
        return r;
    }
    sqlite3_finalize(stmt);

    int64_t uid = sqlite3_last_insert_rowid(_db);

    std::random_device rd;
    std::stringstream token;
    for (int i = 0; i < 32; ++i)
    {
        token << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (rd() & 0xff);
    }

    AuthResult r;
    r.error = 0;
    r.uid = static_cast<int>(uid);
    r.token = token.str();
    r.username = username;
    return r;
}

AuthResult SQLiteMgr::LoginUser(const std::string &username, const std::string &password_hash)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto user = GetUserByUsername_unlocked(username);
    if (!user.has_value())
    {
        AuthResult r;
        r.error = 1007;
        return r;
    }

    if (user->password_hash != password_hash)
    {
        AuthResult r;
        r.error = 1006;
        return r;
    }

    std::random_device rd;
    std::stringstream token;
    for (int i = 0; i < 32; ++i)
    {
        token << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (rd() & 0xff);
    }

    AuthResult r;
    r.error = 0;
    r.uid = user->uid;
    r.token = token.str();
    r.username = user->username;
    return r;
}

bool SQLiteMgr::SendVerifyCode(const std::string &email)
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::random_device rd;
    int code = rd() % 900000 + 100000;

    const char *del_sql = "DELETE FROM verify_codes WHERE email = ?";
    sqlite3_stmt *del_stmt;
    if (sqlite3_prepare_v2(_db, del_sql, -1, &del_stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }

    const char *ins_sql = "INSERT INTO verify_codes (email, code, created_at, expires_at) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *ins_stmt;
    if (sqlite3_prepare_v2(_db, ins_sql, -1, &ins_stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    int64_t now = time(nullptr);
    sqlite3_bind_text(ins_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(ins_stmt, 2, code);
    sqlite3_bind_int64(ins_stmt, 3, now);
    sqlite3_bind_int64(ins_stmt, 4, now + 600);

    bool success = sqlite3_step(ins_stmt) == SQLITE_DONE;
    sqlite3_finalize(ins_stmt);

    spdlog::info("[Auth] VerifyCode for {}: {}", email, code);

    return success;
}

int SQLiteMgr::CheckVerifyCode(const std::string &email, const std::string &code)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql = "SELECT expires_at FROM verify_codes WHERE email = ? AND code = ? ORDER BY id DESC LIMIT 1";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 1003;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int64_t expires_at = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);

        if (time(nullptr) > expires_at)
        {
            return 1003;
        }
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1004;
}

bool SQLiteMgr::ResetPassword(
    const std::string &username, const std::string &email, const std::string &code,
    const std::string &new_password_hash)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto user = GetUserByUsername_unlocked(username);
    if (!user.has_value())
    {
        return false;
    }

    if (user->email != email)
    {
        return false;
    }

    int verify_result = CheckVerifyCode_unlocked(email, code);
    if (verify_result != 0)
    {
        return false;
    }

    const char *sql = "UPDATE users SET password_hash = ? WHERE uid = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, new_password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user->uid);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    if (success)
    {
        const char *del_sql = "DELETE FROM verify_codes WHERE email = ?";
        sqlite3_stmt *del_stmt;
        if (sqlite3_prepare_v2(_db, del_sql, -1, &del_stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(del_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del_stmt);
            sqlite3_finalize(del_stmt);
        }
    }

    return success;
}

std::vector<ChatMessage> SQLiteMgr::SearchMessages(int uid1, int uid2, const std::string &keyword, int limit)
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<ChatMessage> messages;
    const char *sql = R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM messages 
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND content LIKE ?
        ORDER BY timestamp DESC 
        LIMIT ?
    )";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
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

    sqlite3_finalize(stmt);
    return messages;
}

bool SQLiteMgr::SaveUser(const User &user)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql =
        "INSERT INTO users (username, password_hash, email, avatar_path, created_at) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user.password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user.avatar_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, user.created_at);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

std::optional<User> SQLiteMgr::GetUserByUsername(const std::string &username)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql =
        "SELECT uid, username, password_hash, email, avatar_path, created_at FROM users WHERE username = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    User user;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        user.uid = sqlite3_column_int(stmt, 0);
        user.username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        user.password_hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        user.email = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        user.avatar_path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        user.created_at = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<User> SQLiteMgr::GetUserByUsername_unlocked(const std::string &username)
{
    const char *sql =
        "SELECT uid, username, password_hash, email, avatar_path, created_at FROM users WHERE username = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    User user;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        user.uid = sqlite3_column_int(stmt, 0);
        user.username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        user.password_hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        user.email = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        user.avatar_path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        user.created_at = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);
        return user;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

int SQLiteMgr::CheckVerifyCode_unlocked(const std::string &email, const std::string &code)
{
    const char *sql = "SELECT expires_at FROM verify_codes WHERE email = ? AND code = ? ORDER BY id DESC LIMIT 1";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 1003;
    }
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int64_t expires_at = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        if (time(nullptr) > expires_at)
        {
            return 1003;
        }
        return 0;
    }
    sqlite3_finalize(stmt);
    return 1004;
}

std::optional<User> SQLiteMgr::GetUserByUid(int uid)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql = "SELECT uid, username, password_hash, email, avatar_path, created_at FROM users WHERE uid = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, uid);

    User user;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        user.uid = sqlite3_column_int(stmt, 0);
        user.username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        user.password_hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        user.email = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
        user.avatar_path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        user.created_at = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool SQLiteMgr::UpdateUserAvatar(int uid, const std::string &avatar_path)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql = "UPDATE users SET avatar_path = ? WHERE uid = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, avatar_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, uid);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

bool SQLiteMgr::SaveOfflineMessage(const ChatMessage &msg)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql =
        "INSERT INTO offline_messages (from_uid, to_uid, content, timestamp, status) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

std::vector<ChatMessage> SQLiteMgr::GetOfflineMessages(int uid)
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<ChatMessage> messages;
    const char *sql = R"(
        SELECT id, from_uid, to_uid, content, timestamp, status 
        FROM offline_messages 
        WHERE to_uid = ?
        ORDER BY timestamp ASC
    )";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
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

    sqlite3_finalize(stmt);
    return messages;
}

bool SQLiteMgr::ClearOfflineMessages(int uid)
{
    std::lock_guard<std::mutex> lock(_mutex);

    const char *sql = "DELETE FROM offline_messages WHERE to_uid = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

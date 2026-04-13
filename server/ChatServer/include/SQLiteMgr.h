#ifndef SQLITE_MGR_H
#define SQLITE_MGR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

struct ChatMessage
{
    int64_t id;
    int from_uid;
    int to_uid;
    std::string content;
    int64_t timestamp;
    int status;
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
    int error;
    int uid;
    std::string token;
    std::string username;
};

class SQLiteMgr
{
public:
    static SQLiteMgr &Instance();

    bool Init(const std::string &db_path);
    void Shutdown();

    bool SaveMessage(const ChatMessage &msg);
    std::vector<ChatMessage> GetMessages(int uid1, int uid2, int64_t before_time, int limit = 50);
    std::vector<ChatMessage> SearchMessages(int uid1, int uid2, const std::string &keyword, int limit = 50);

    bool SaveUser(const User &user);
    std::optional<User> GetUserByUsername(const std::string &username);
    std::optional<User> GetUserByUid(int uid);
    bool UpdateUserAvatar(int uid, const std::string &avatar_path);

    bool SaveOfflineMessage(const ChatMessage &msg);
    std::vector<ChatMessage> GetOfflineMessages(int uid);
    bool ClearOfflineMessages(int uid);

    AuthResult RegisterUser(const std::string &username, const std::string &password_hash, const std::string &email);
    AuthResult LoginUser(const std::string &username, const std::string &password_hash);
    bool SendVerifyCode(const std::string &email);
    int CheckVerifyCode(const std::string &email, const std::string &code);
    bool ResetPassword(
        const std::string &username, const std::string &email, const std::string &code,
        const std::string &new_password_hash);

    SQLiteMgr(const SQLiteMgr &) = delete;
    SQLiteMgr &operator=(const SQLiteMgr &) = delete;

private:
    SQLiteMgr() = default;
    ~SQLiteMgr();

    bool CreateTables();
    std::optional<User> GetUserByUsername_unlocked(const std::string &username);
    int CheckVerifyCode_unlocked(const std::string &email, const std::string &code);

    sqlite3 *_db = nullptr;
    std::mutex _mutex;
    std::atomic<bool> _initialized{false};
};

#endif

#ifndef USER_DATA_H
#define USER_DATA_H

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

class UserData
{
public:
    static UserData& Instance();
    
    bool Load(const std::string& filepath);
    bool Save(const std::string& filepath);
    
    bool AddUser(int uid, const std::string& username, const std::string& password_hash);
    std::optional<std::string> GetPasswordHash(const std::string& username);
    std::optional<int> GetUid(const std::string& username);
    std::optional<std::string> GetUsername(int uid);
    
    UserData(const UserData&) = delete;
    UserData& operator=(const UserData&) = delete;

private:
    UserData() = default;
    
    std::unordered_map<std::string, int> _username_to_uid;
    std::unordered_map<int, std::string> _uid_to_username;
    std::unordered_map<int, std::string> _uid_to_password;
    std::unordered_map<int, std::string> _uid_to_avatar;
    
    json _data;
    std::mutex _mutex;
};

#endif

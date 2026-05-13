/**
 * @file UserData.cpp
 * @brief 用户数据内存缓存实现
 * @details 从 JSON 文件加载用户数据，支持按用户名/UID 查找。
 */
#include "UserData.h"
#include <fstream>

/**
 * @brief 获取单例实例
 */
UserData& UserData::Instance()
{
    static UserData instance;
    return instance;
}

/**
 * @brief 从 JSON 文件加载用户数据
 * @param filepath 文件路径
 * @return 成功返回 true
 */
bool UserData::Load(const std::string& filepath)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            _data = json::object();
            return true;
        }
        
        file >> _data;
        
        if (_data.contains("users")) {
            for (const auto& user : _data["users"].items()) {
                int uid = user.value().at("uid").get<int>();
                std::string username = user.value().at("username").get<std::string>();
                std::string password_hash = user.value().at("password_hash").get<std::string>();
                std::string avatar_path = user.value().value("avatar_path", "");
                
                _username_to_uid[username] = uid;
                _uid_to_username[uid] = username;
                _uid_to_password[uid] = password_hash;
                _uid_to_avatar[uid] = avatar_path;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief 保存用户数据到 JSON 文件
 * @param filepath 文件路径
 * @return 成功返回 true
 */
bool UserData::Save(const std::string& filepath)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    try {
        json users = json::array();
        
        for (const auto& [uid, username] : _uid_to_username) {
            json user;
            user["uid"] = uid;
            user["username"] = username;
            user["password_hash"] = _uid_to_password[uid];
            user["avatar_path"] = _uid_to_avatar[uid];
            users.push_back(user);
        }
        
        _data["users"] = users;
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        file << _data.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief 添加用户到缓存
 * @param uid 用户 ID
 * @param username 用户名
 * @param password_hash 密码哈希
 * @return 添加成功返回 true（用户名已存在返回 false）
 */
bool UserData::AddUser(int uid, const std::string& username, const std::string& password_hash)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (_username_to_uid.find(username) != _username_to_uid.end()) {
        return false;
    }
    
    _username_to_uid[username] = uid;
    _uid_to_username[uid] = username;
    _uid_to_password[uid] = password_hash;
    _uid_to_avatar[uid] = "";
    
    return true;
}

/**
 * @brief 通过用户名获取密码哈希
 * @param username 用户名
 * @return 密码哈希，不存在则返回 std::nullopt
 */
std::optional<std::string> UserData::GetPasswordHash(const std::string& username)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _username_to_uid.find(username);
    if (it == _username_to_uid.end()) {
        return std::nullopt;
    }
    
    int uid = it->second;
    auto password_it = _uid_to_password.find(uid);
    if (password_it == _uid_to_password.end()) {
        return std::nullopt;
    }
    
    return password_it->second;
}

/**
 * @brief 通过用户名获取 UID
 * @param username 用户名
 * @return UID，不存在则返回 std::nullopt
 */
std::optional<int> UserData::GetUid(const std::string& username)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _username_to_uid.find(username);
    if (it == _username_to_uid.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

/**
 * @brief 通过 UID 获取用户名
 * @param uid 用户 ID
 * @return 用户名，不存在则返回 std::nullopt
 */
std::optional<std::string> UserData::GetUsername(int uid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _uid_to_username.find(uid);
    if (it == _uid_to_username.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

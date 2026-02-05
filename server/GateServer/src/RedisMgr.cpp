#include "RedisMgr.h"

RedisMgr::RedisMgr()
{
}

RedisMgr::~RedisMgr()
{
}

// Mock 连接：永远返回成功
bool RedisMgr::Connect(const std::string &host, int port)
{
    std::cout << "[Mock Redis] Connect to " << host << ":" << port << " Success." << std::endl;
    return true;
}

// Mock Auth：永远通过
bool RedisMgr::Auth(const std::string &password)
{
    std::cout << "[Mock Redis] Auth Success." << std::endl;
    return true;
}

bool RedisMgr::Get(const std::string &key, std::string &value)
{
    // 【万能后门】
    // 只要是查验证码，不管什么邮箱，永远告诉 LogicSystem：Redis 里存的是 "123456"
    value = "123456";
    std::cout << "[Mock Redis] Get " << key << " -> always return 123456" << std::endl;
    return true;
}

// 核心功能：用 map 模拟 SET
bool RedisMgr::Set(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _string_cache[key] = value;
    std::cout << "[Mock Redis] SET " << key << " = " << value << std::endl;
    return true;
}

// 还有其他接口，暂时给个空实现，用到再补
bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
    return true;
}
bool RedisMgr::LPop(const std::string &key, std::string &value)
{
    return true;
}
bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value)
{
    return true;
}
std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
    return "";
}
bool RedisMgr::Del(const std::string &key)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _string_cache.erase(key);
    return true;
}
bool RedisMgr::ExistsKey(const std::string &key)
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _string_cache.find(key) != _string_cache.end();
}
void RedisMgr::Close()
{
}
#include "RedisMgr.h"

RedisMgr::RedisMgr()
{
}

RedisMgr::~RedisMgr()
{
}

bool RedisMgr::Connect(const std::string &host, int port)
{
    // 模拟连接成功
    std::cout << "[Mock Redis] Connect to " << host << ":" << port << " Success." << std::endl;
    return true;
}

bool RedisMgr::Auth(const std::string &password)
{
    std::cout << "[Mock Redis] Auth Success." << std::endl;
    return true;
}

bool RedisMgr::Get(const std::string &key, std::string &value)
{
    // 模拟获取验证码，始终返回测试值
    value = "123456";
    std::cout << "[Mock Redis] Get " << key << " -> always return 123456" << std::endl;
    return true;
}

bool RedisMgr::Set(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _string_cache[key] = value;
    std::cout << "[Mock Redis] SET " << key << " = " << value << std::endl;
    return true;
}

// 预留接口实现
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
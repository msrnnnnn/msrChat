#include "RedisMgr.h"
#include <spdlog/spdlog.h>

RedisMgr::RedisMgr()
{
}

RedisMgr::~RedisMgr()
{
}

bool RedisMgr::Connect(const std::string &host, int port)
{
    // 模拟连接成功
    spdlog::info("[Mock Redis] Connect to {}:{} Success.", host, port);
    return true;
}

bool RedisMgr::Auth(const std::string &password)
{
    spdlog::info("[Mock Redis] Auth Success.");
    return true;
}

bool RedisMgr::Get(const std::string &key, std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _string_cache.find(key);
    if (it == _string_cache.end())
    {
        return false;
    }
    value = it->second;
    spdlog::info("[Mock Redis] Get {} -> {}", key, value);
    return true;
}

bool RedisMgr::Set(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _string_cache[key] = value;
    spdlog::info("[Mock Redis] SET {} = {}", key, value);
    return true;
}

// 预留接口实现
bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _list_cache[key].push_front(value);
    return true;
}
bool RedisMgr::LPop(const std::string &key, std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _list_cache.find(key);
    if (it == _list_cache.end() || it->second.empty())
    {
        return false;
    }
    value = it->second.front();
    it->second.pop_front();
    return true;
}
bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _hash_cache[key][hkey] = value;
    return true;
}
std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto kit = _hash_cache.find(key);
    if (kit == _hash_cache.end())
    {
        return "";
    }
    auto hit = kit->second.find(hkey);
    if (hit == kit->second.end())
    {
        return "";
    }
    return hit->second;
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

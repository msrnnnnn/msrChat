#include "RedisMgr.h"
#include <spdlog/spdlog.h>

RedisMgr::RedisMgr() : _connect(nullptr), _reply(nullptr)
{
}

RedisMgr::~RedisMgr()
{
    Close();
}

bool RedisMgr::Connect(const std::string &host, int port)
{
    std::lock_guard<std::mutex> lock(_mtx);
    Close(); // Ensure clean slate
    _connect = redisConnect(host.c_str(), port);
    if (_connect == nullptr || _connect->err)
    {
        if (_connect)
        {
            spdlog::error("Redis connection error: {}", _connect->errstr);
            redisFree(_connect);
            _connect = nullptr;
        }
        else
        {
            spdlog::error("Redis connection error: can't allocate context");
        }
        return false;
    }
    spdlog::info("Redis Connect Success: {}:{}", host, port);
    return true;
}

bool RedisMgr::Auth(const std::string &password)
{
    if (password.empty()) return true; // No password needed

    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;

    _reply = (redisReply *)redisCommand(_connect, "AUTH %s", password.c_str());
    if (!_reply) {
        spdlog::error("Redis Auth failed: Command error");
        return false;
    }

    bool success = false;
    if (_reply->type == REDIS_REPLY_STATUS && std::string(_reply->str) == "OK") {
        success = true;
        spdlog::info("Redis Auth Success");
    } else {
        spdlog::error("Redis Auth failed: {}", _reply->str ? _reply->str : "Unknown error");
    }
    
    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::Get(const std::string &key, std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;

    _reply = (redisReply *)redisCommand(_connect, "GET %s", key.c_str());
    if (!_reply) {
        spdlog::error("Redis Get failed: Command error");
        return false;
    }

    bool success = false;
    if (_reply->type == REDIS_REPLY_STRING) {
        value = _reply->str;
        success = true;
    } else if (_reply->type == REDIS_REPLY_NIL) {
        success = false; // Not found
        spdlog::warn("Redis Get: Key {} not found", key);
    } else {
         spdlog::error("Redis Get error: type {}", _reply->type);
    }

    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::Set(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;

    _reply = (redisReply *)redisCommand(_connect, "SET %s %s", key.c_str(), value.c_str());
    if (!_reply) {
         spdlog::error("Redis Set failed: Command error");
         return false;
    }
    
    bool success = false;
    // SET returns OK status on success
    if (_reply->type == REDIS_REPLY_STATUS && std::string(_reply->str) == "OK") {
        success = true;
    } else {
        spdlog::error("Redis Set failed: {}", _reply->str ? _reply->str : "Unknown error");
    }

    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::SetEx(const std::string &key, const std::string &value, int timeout)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;
    
    _reply = (redisReply *)redisCommand(_connect, "SETEX %s %d %s", key.c_str(), timeout, value.c_str());
     if (!_reply) {
         spdlog::error("Redis SetEx failed: Command error");
         return false;
    }

    bool success = false;
    if (_reply->type == REDIS_REPLY_STATUS && std::string(_reply->str) == "OK") {
        success = true;
    } else {
        spdlog::error("Redis SetEx failed: {}", _reply->str ? _reply->str : "Unknown error");
    }
    
    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::LPush(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;
    
    _reply = (redisReply *)redisCommand(_connect, "LPUSH %s %s", key.c_str(), value.c_str());
    if (!_reply) return false;
    
    bool success = (_reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::LPop(const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;
    
    _reply = (redisReply *)redisCommand(_connect, "LPOP %s", key.c_str());
    if (!_reply) return false;
    
    bool success = false;
    if (_reply->type == REDIS_REPLY_STRING) {
        value = _reply->str;
        success = true;
    }
    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value) {
     std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;
    
    _reply = (redisReply *)redisCommand(_connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
    if (!_reply) return false;
    
    // HSET returns integer (1 created, 0 updated)
    bool success = (_reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

std::string RedisMgr::HGet(const std::string &key, const std::string &hkey) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return "";
    
    _reply = (redisReply *)redisCommand(_connect, "HGET %s %s", key.c_str(), hkey.c_str());
    if (!_reply) return "";
    
    std::string res = "";
    if (_reply->type == REDIS_REPLY_STRING) {
        res = _reply->str;
    }
    freeReplyObject(_reply);
    _reply = nullptr;
    return res;
}

bool RedisMgr::Del(const std::string &key) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;
    
    _reply = (redisReply *)redisCommand(_connect, "DEL %s", key.c_str());
    if (!_reply) return false;
    
    bool success = (_reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(_reply);
    _reply = nullptr;
    return success;
}

bool RedisMgr::ExistsKey(const std::string &key) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_connect) return false;
    
    _reply = (redisReply *)redisCommand(_connect, "EXISTS %s", key.c_str());
    if (!_reply) return false;
    
    bool exists = false;
    if (_reply->type == REDIS_REPLY_INTEGER && _reply->integer > 0) {
        exists = true;
    }
    freeReplyObject(_reply);
    _reply = nullptr;
    return exists;
}

void RedisMgr::Close()
{
    if (_connect) {
        redisFree(_connect);
        _connect = nullptr;
        spdlog::info("Redis connection closed");
    }
}

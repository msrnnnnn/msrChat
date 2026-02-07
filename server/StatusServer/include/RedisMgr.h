/**
 * @file RedisMgr.h
 * @brief Redis 管理器定义 (Mock 实现)
 * @author msr
 */

#pragma once

#include "Singleton.h"
#include <hiredis/hiredis.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <queue>
#include <atomic>

/**
 * @class   RedisMgr
 * @brief   Redis Manager (Real Implementation via hiredis)
 *
 * @details Thread-safe Redis client wrapper using hiredis.
 *          Uses a mutex to protect the single redisContext (Simple Thread-Safe).
 *          For higher concurrency, a connection pool should be implemented.
 */
class RedisMgr : public Singleton<RedisMgr>, public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;

public:
    ~RedisMgr();

    /**
     * @brief   Connect to Redis
     * @param   host Hostname/IP
     * @param   port Port
     * @return  bool Success
     */
    bool Connect(const std::string &host, int port);

    /**
     * @brief   Authenticate
     * @param   password Password
     * @return  bool Success
     */
    bool Auth(const std::string &password);

    /**
     * @brief   Get Value by Key
     * @param   key   Key
     * @param   value Output value
     * @return  bool  Success (true if found, false if error or nil)
     */
    bool Get(const std::string &key, std::string &value);

    /**
     * @brief   Set Key-Value
     * @param   key   Key
     * @param   value Value
     * @return  bool  Success
     */
    bool Set(const std::string &key, const std::string &value);

    /**
     * @brief   Set Key-Value with Expiration
     * @param   key     Key
     * @param   value   Value
     * @param   timeout Expiration in seconds
     * @return  bool    Success
     */
    bool SetEx(const std::string &key, const std::string &value, int timeout);

    /**
     * @brief   Left Push to List
     * @param   key   Key
     * @param   value Value
     * @return  bool  Success
     */
    bool LPush(const std::string &key, const std::string &value);

    /**
     * @brief   Left Pop from List
     * @param   key   Key
     * @param   value Output value
     * @return  bool  Success
     */
    bool LPop(const std::string &key, std::string &value);

    /**
     * @brief   Hash Set
     * @param   key   Key
     * @param   hkey  Hash Key
     * @param   value Value
     * @return  bool  Success
     */
    bool HSet(const std::string &key, const std::string &hkey, const std::string &value);

    /**
     * @brief   Hash Get
     * @param   key   Key
     * @param   hkey  Hash Key
     * @return  std::string Value
     */
    std::string HGet(const std::string &key, const std::string &hkey);

    /**
     * @brief   Delete Key
     * @param   key   Key
     * @return  bool  Success
     */
    bool Del(const std::string &key);

    /**
     * @brief   Check if Key exists
     * @param   key   Key
     * @return  bool  Exists
     */
    bool ExistsKey(const std::string &key);

    /**
     * @brief   Close connection
     */
    void Close();

private:
    RedisMgr();

    redisContext* _connect; ///< hiredis context
    redisReply* _reply;     ///< hiredis reply
    std::mutex _mtx;        ///< Mutex for thread safety
};

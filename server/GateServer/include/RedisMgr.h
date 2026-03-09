/**
 * @file RedisMgr.h
 * @brief Redis 管理器定义
 * @details 封装 Redis 操作接口，提供统一的缓存服务。
 */
#pragma once

#include "Singleton.h"
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * @class   RedisMgr
 * @brief   Redis 管理器 (Singleton)
 */
class RedisMgr : public Singleton<RedisMgr>, public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;

public:
    ~RedisMgr();

    /**
     * @brief   连接 Redis
     * @param   host 主机地址
     * @param   port 端口号
     * @return  bool 连接状态
     */
    bool Connect(const std::string &host, int port);

    /**
     * @brief   认证
     * @param   password 密码
     * @return  bool 认证状态
     */
    bool Auth(const std::string &password);

    /**
     * @brief   获取 Key 对应的值
     * @param   key   键
     * @param   value 输出参数，存储获取到的值
     * @return  bool  获取是否成功
     */
    bool Get(const std::string &key, std::string &value);

    /**
     * @brief   设置 Key-Value
     * @param   key   键
     * @param   value 值
     * @return  bool  设置是否成功
     */
    bool Set(const std::string &key, const std::string &value);

    /**
     * @brief   列表左推入
     * @param   key   键
     * @param   value 值
     * @return  bool  操作是否成功
     */
    bool LPush(const std::string &key, const std::string &value);

    /**
     * @brief   列表左弹出
     * @param   key   键
     * @param   value 输出参数，存储弹出的值
     * @return  bool  操作是否成功
     */
    bool LPop(const std::string &key, std::string &value);

    /**
     * @brief   哈希设置
     * @param   key   键
     * @param   hkey  哈希键
     * @param   value 值
     * @return  bool  操作是否成功
     */
    bool HSet(const std::string &key, const std::string &hkey, const std::string &value);

    /**
     * @brief   哈希获取
     * @param   key   键
     * @param   hkey  哈希键
     * @return  std::string 获取到的值
     */
    std::string HGet(const std::string &key, const std::string &hkey);

    /**
     * @brief   删除 Key
     * @param   key   键
     * @return  bool  删除是否成功
     */
    bool Del(const std::string &key);

    /**
     * @brief   判断 Key 是否存在
     * @param   key   键
     * @return  bool  是否存在
     */
    bool ExistsKey(const std::string &key);

    /**
     * @brief   关闭连接
     */
    void Close();

private:
    RedisMgr();

    std::unordered_map<std::string, std::string> _string_cache; ///< 模拟 Redis 存储
    std::mutex _mtx;                                            ///< 保护 map 线程安全
};
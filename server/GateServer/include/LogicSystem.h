#pragma once
#include "Singleton.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class HttpConnection;
// 定义 HTTP 处理函数签名：接受一个 HttpConnection 的智能指针
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

/**
 * @class LogicSystem
 * @brief 业务逻辑分发系统 (Singleton)
 * @details 维护 URL 到处理函数 (Handler) 的映射表。
 */
class LogicSystem : public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;

public:
    ~LogicSystem() = default;

    /**
     * @brief 查找并执行 GET 请求对应的 Handler
     * @return true 找到路由并执行; false 未找到路由
     */
    bool HandleGet(std::string path, std::shared_ptr<HttpConnection> connection);

    /**
     * @brief 注册 GET 路由
     * @param url 路径 (如 "/login")
     * @param handler 回调函数 lambda
     */
    void RegisterGet(std::string url, HttpHandler handler);

    bool HandlePost(std::string path, std::shared_ptr<HttpConnection> connection);
    void RegisterPost(std::string url, HttpHandler handler);

private:
    LogicSystem();
    // 使用 Hash Map 存储路由表，查询时间复杂度 O(1)
    std::unordered_map<std::string, HttpHandler> _registerPost;
    std::unordered_map<std::string, HttpHandler> _registerGet;
};
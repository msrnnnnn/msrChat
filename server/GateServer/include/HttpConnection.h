/**
 * @file    HttpConnection.h
 * @brief   HTTP 连接类声明
 * @author  msr
 *
 * @details 管理单个 HTTP 连接的生命周期，处理请求解析、响应发送和超时检测。
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <unordered_map>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

class LogicSystem;

/**
 * @class   HttpConnection
 * @brief   HTTP 会话管理器 (Session)
 *
 * @details
 * 每个 TCP 连接对应一个 HttpConnection 实例。
 * 负责 HTTP 协议解析、超时检测、请求分发。
 * 生命周期由 `shared_ptr` 自我维持，直到连接断开。
 */
class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
    friend class LogicSystem;

public:
    /**
     * @brief   构造函数
     * @param   socket 已连接的套接字 (Connected Socket)，所有权被转移至此。
     */
    HttpConnection(tcp::socket &&socket);

    /**
     * @brief   启动异步读取流程
     */
    void Start();

private:
    /**
     * @brief   超时检测协程 (Watchdog)
     * @details 防止 Slowloris 攻击（客户端建立连接后不发数据，耗尽服务器资源）。
     */
    void CheckDeadline();

    void WriteResponse();
    void HandleRequest();
    void PreParseGetParam();

    tcp::socket _socket;

    /**
     * @brief   Beast 动态缓冲区 (Dynamic Buffer)
     * @note
     * 1. 自动扩容。
     * 2. 处理 TCP 粘包/拆包的核心组件。
     * 3. `async_read` 读多了的数据会留在这里供下一次解析使用。
     */
    beast::flat_buffer _buffer{8192};

    /**
     * @brief   HTTP 请求对象
     * @tparam  dynamic_body 表示 Body 可以是字符串、文件等动态内容。
     */
    http::request<http::dynamic_body> _request;

    /**
     * @brief   HTTP 响应对象
     */
    http::response<http::dynamic_body> _response;

    std::string _get_url;
    std::unordered_map<std::string, std::string> _get_params;

    /**
     * @brief   截止时间定时器
     * @warning 必须与 _socket 绑定在同一个 io_context (executor) 上，避免线程安全问题。
     */
    net::steady_timer deadline_{_socket.get_executor(), std::chrono::seconds(60)};
};

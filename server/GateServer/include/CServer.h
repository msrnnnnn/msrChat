/**
 * @file    CServer.h
 * @brief   TCP 服务器类声明
 * @author  msr
 *
 * @details 定义了 TCP 连接接收器，负责接受新连接并分发给 HttpConnection 处理。
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <memory>

namespace net = boost::asio;
namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

/**
 * @class   CServer
 * @brief   TCP 连接接收器 (Acceptor)
 *
 * @details
 * 继承 `enable_shared_from_this` 是为了在异步回调链中保活 (Keep-Alive)。
 * 它的唯一职责是将新连接 (Socket) 移交给 `HttpConnection` 处理。
 */
class CServer : public std::enable_shared_from_this<CServer>
{
public:
    /**
     * @brief   构造函数
     * @param   ioc  IO 上下文引用，用于注册 Accept 事件
     * @param   port 监听端口
     */
    CServer(net::io_context &ioc, unsigned short &port);

    /**
     * @brief   启动异步接收循环
     * @note    这是一个递归链的起点。
     */
    void HandleAccept();

private:
    /**
     * @brief   内核监听 Socket
     * @note    对应内核状态: LISTEN。用于接收 TCP 三次握手完成后的连接。
     */
    tcp::acceptor _acceptor;

    net::io_context &_ioc;

    /**
     * @warning [CRITICAL ARCHITECTURAL HAZARD] (架构级隐患)
     * @details
     * 将 `socket` 作为成员变量复用是极其危险的 "Sync-Mindset" (同步思维) 遗毒。
     * 1. 当 `async_accept` 成功后，你使用 `std::move(_socket)` 将其转移给了 Session。
     * 2. 此时 `_socket` 处于 "Moved-from state" (未定义/空壳状态)。
     * 3. 虽然你在下一次 `HandleAccept` 中再次使用它，这依赖于 Acceptor 内部实现的容错性。
     * @todo    修复方案: 应该在 HandleAccept 内部创建 `socket` 的局部智能指针，或由 Session 创建。
     */
    tcp::socket _socket;
};

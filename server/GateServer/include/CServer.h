/**
 * @file    CServer.h
 * @brief   TCP 服务器类声明
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
 * @brief   TCP 连接接收器
 * @details 负责监听端口并接受 TCP 连接，随后将 socket 移交给 HttpConnection 处理。
 */
class CServer : public std::enable_shared_from_this<CServer>
{
public:
    CServer(net::io_context &ioc, unsigned short &port);
    void HandleAccept();

private:
    tcp::acceptor _acceptor;
    net::io_context &_ioc;
    tcp::socket _socket;
};

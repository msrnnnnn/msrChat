/**
 * @file    CServer.cpp
 * @brief   TCP 服务器类实现
 */

#include "CServer.h"
#include "AsioIOServicePool.h"
#include "HttpConnection.h"
#include <iostream>

CServer::CServer(net::io_context &ioc, unsigned short &port)
    : _ioc(ioc),
      _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
      _socket(ioc)
{
}

void CServer::HandleAccept()
{
    // 获取 shared_from_this 以确保在异步回调中对象存活
    auto self = shared_from_this();

    auto ioc_ptr = AsioIOServicePool::GetInstance()->GetIOService();

    // 为新连接创建 socket，绑定到 IO 线程池中的某个 io_context
    auto new_socket = std::make_shared<tcp::socket>(*ioc_ptr);

    // 提交异步 Accept 请求
    _acceptor.async_accept(
        *new_socket,
        [self, new_socket](beast::error_code ec)
        {
            try
            {
                // 即使 Accept 失败（如文件描述符耗尽），也要继续监听，否则服务器会停止服务
                if (ec)
                {
                    self->HandleAccept();
                    return;
                }

                std::make_shared<HttpConnection>(std::move(*new_socket))->Start();

                // 继续监听下一个连接
                self->HandleAccept();
            }
            catch (std::exception &exp)
            {
                std::cout << "exception is" << exp.what() << std::endl;
                self->HandleAccept();
            }
        });
}

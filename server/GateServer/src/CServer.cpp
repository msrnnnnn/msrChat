/**
 * @file    CServer.cpp
 * @brief   TCP 服务器类实现
 * @author  msr
 */

#include "CServer.h"
#include "AsioIOServicePool.h"
#include "HttpConnection.h"
#include <iostream>

/**
 * @brief   构造函数
 * @details
 * _acceptor执行序列:
 * 1. socket(AF_INET, SOCK_STREAM, 0) -> 创建文件描述符
 * 2. setsockopt(SO_REUSEADDR) -> 允许地址复用 (Beast 默认开启)
 * 3. bind(port) -> 绑定端口
 * 4. listen() -> 标记为被动套接字
 */
CServer::CServer(net::io_context &ioc, unsigned short &port)
    : _ioc(ioc),
      _acceptor(ioc, tcp::endpoint(tcp::v4(), port)), // endpoint相当于填充了 C 语言中的 struct sockaddr_in 结构体
      _socket(ioc)
{
}

void CServer::HandleAccept()
{
    // [LifeCycle] 捕获 self (shared_ptr)，引用计数 +1。
    // 确保异步回调执行时，CServer 对象没有被析构。
    auto self = shared_from_this();

    auto ioc_ptr = AsioIOServicePool::GetInstance()->GetIOService();

    auto new_socket = std::make_shared<tcp::socket>(*ioc_ptr);

    /**
     * @brief 提交异步 Accept 请求到内核 (Epoll/IOCP)
     * @param _socket 用于存放新连接 fd 的对象 (即使它现在可能是空的/无效的)
     * @param callback 当三次握手完成 (SYN Queue -> Accept Queue)，内核触发此回调
     */
    _acceptor.async_accept(
        *new_socket,
        [self, new_socket](beast::error_code ec)
        {
            try
            {
                // [Error Handling] 即使 Accept 失败（如文件描述符耗尽 EMFILE），也要继续监听，否则服务器会停止服务。
                if (ec)
                {
                    self->HandleAccept();
                    return;
                }

                std::make_shared<HttpConnection>(std::move(*new_socket))->Start();

                // [Recursion] 继续监听下一个连接
                self->HandleAccept();
            }
            catch (std::exception &exp)
            {
                std::cout << "exception is" << exp.what() << std::endl;
                self->HandleAccept();
            }
        });
}

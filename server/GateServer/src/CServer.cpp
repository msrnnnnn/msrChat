#include "CServer.h"
#include "HttpConnection.h"
#include <iostream>

/**
 * @brief 初始化服务器并绑定端口
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

    /**
     * @brief 提交异步 Accept 请求到内核 (Epoll/IOCP)
     * @param _socket 用于存放新连接 fd 的对象 (即使它现在可能是空的/无效的)
     * @param callback 当三次握手完成 (SYN Queue -> Accept Queue)，内核触发此回调
     */
    _acceptor.async_accept(
        _socket,
        [self](beast::error_code ec)
        {
            try
            {
                // [Error Handling] 即使 Accept 失败（如文件描述符耗尽 EMFILE），也要继续监听，否则服务器会停止服务。
                if (ec)
                {
                    self->HandleAccept();
                    return;
                }

                /**
                 * @warning [Ownership Transfer] (所有权转移)
                 * `std::move(self->_socket)` 将 Socket (及其底层的 fd) 的所有权
                 * 彻底转移给了 `HttpConnection`。
                 * 此时 `self->_socket` 变为空壳。
                 * 下一次 `self->HandleAccept()` 将使用这个空壳 socket，这是极其危险的写法。
                 */
                std::make_shared<HttpConnection>(std::move(self->_socket))->ReadRequest();

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
#include "CServer.h"
#include <iostream>

CServer::CServer(net::io_context &ioc, unsigned short &port)
    : _ioc(ioc),
      _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
      _socket(ioc)
{
}

void CServer::Start()
{
    auto self = shared_from_this();
    _acceptor.async_accept(
        _socket,
        [self](beast::error_code error)
        {
            try
            {
                if (error)
                {
                    self->Start();
                    return;
                }
                std::make_shared<HttpConnection>(std::move(_socket))->Start();
                self->Start();
            }
            catch (std::exception &exp)
            {
                std::cout << "exception is" << exp.what() << std::endl;
                self->Start();
            }
        });
}
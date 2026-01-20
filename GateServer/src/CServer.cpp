#include "CServer.h"
#include <iostream>

CServer::CServer(net::io_context &ioc, unsigned short &port)
    : _ioc(ioc),
      _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
      _socket(ioc)
{
}

void CServer::HandleAccept()
{
    auto self = shared_from_this();
    _acceptor.async_accept(
        _socket,
        [self](beast::error_code ec)
        {
            try
            {
                if (ec)
                {
                    self->HandleAccept();
                    return;
                }
                std::make_shared<HttpConnection>(std::move(_socket))->HandleAccept();
                self->HandleAccept();
            }
            catch (std::exception &exp)
            {
                std::cout << "exception is" << exp.what() << std::endl;
                self->HandleAccept();
            }
        });
}
#include "HttpConnection.h"
#include <iostream>

HttpConnection::HttpConnection(tcp::socket socket)
    : _socket(std::move(socket)){

    }

void HttpConnection::Start(){
    auto self = shared_from_this();
    http::async_read(
        _socket, _buffer, _request,
        [self](beast::error_code ec, std::size_t bytes_transferred)
        {
            try
            {
                if (ec)
                {
                    std::cout << "http read is" << ec.what() << std::endl;
                    return;
                }
                boost::ignore_unused();
                self->CheckDeadline();
            }
            catch (std::exception &exp)
            {
                std::cout << "exception is" << exp.what() << std::endl;
                return;
            }
        });
}
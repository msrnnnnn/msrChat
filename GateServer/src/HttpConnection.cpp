#include "HttpConnection.h"
#include "LogicSystem.h"
#include <iostream>

HttpConnection::HttpConnection(tcp::socket socket)
    : _socket(std::move(socket))
{
}

void HttpConnection::ReadRequest()
{
    auto self = shared_from_this();
    deadline_.expires_after(std::chrono::seconds(60));
    self->CheckDeadline();
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
                boost::ignore_unused(bytes_transferred);
                self->HandleRequest();
            }
            catch (std::exception &exp)
            {
                std::cout << "exception is" << exp.what() << std::endl;
                return;
            }
        });
}

void HttpConnection::HandleRequest()
{
    _response.version(_request.version());
    bool keep_alive = _request.keep_alive();
    _response.keep_alive(keep_alive);
    if (_request.method() == http::verb::get)
    {
        bool success = LogicSystem::GetInstance()->HandleGet(_request.target(), shared_from_this());
        if (!success)
        {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url not found \r\n";
            WriteRespose();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteRespose();
        return;
    }
}

void HttpConnection::WriteRespose()
{
    auto self = shared_from_this();
    _response.content_length(_response.body().size());
    http::async_write(
        _socket, _response,
        [self](beast::error_code ec, std::size_t)
        {
            if (ec)
            {
                self->_socket.shutdown(tcp::socket::shutdown_send, ec);
                return;
            }
            if (self->_response.keep_alive())
            {
                self->_request = {};
                self->_response = {};
                self->ReadRequest();
            }
            else
            {
                self->_socket.shutdown(tcp::socket::shutdown_send, ec);
                self->deadline_.cancel();
            }
        });
}

void HttpConnection::CheckDeadline()

{
    auto self = shared_from_this();
    deadline_.async_wait(

        [self](beast::error_code ec)
        {
            if (!ec)
            {
                self->_socket.close();
            }
        });
}
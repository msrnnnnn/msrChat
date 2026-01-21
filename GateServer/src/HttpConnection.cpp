#include "HttpConnection.h"
#include "LogicSystem.h"
#include <iostream>
#include <string>

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

// 这是一个标准的 URL 解码函数，能处理 %20 和 + 号
unsigned char ToHex(unsigned char x)
{
    return x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z')
        y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z')
        y = x - 'a' + 10;
    else if (x >= '0' && x <= '9')
        y = x - '0';
    else
        assert(0);
    return y;
}

std::string UrlDecode(const std::string &str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        // 处理 + 号为空格的情况（视标准而定，有时候需要，有时候不需要，通常保留比较安全）
        if (str[i] == '+')
            strTemp += ' ';
        // 处理 %xx 格式
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else
            strTemp += str[i];
    }
    return strTemp;
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
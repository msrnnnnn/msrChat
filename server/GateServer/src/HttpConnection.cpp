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

    // [Heartbeat] 更新超时时间：当前时间 + 60秒
    deadline_.expires_after(std::chrono::seconds(60));

    // 启动/检测超时定时器
    self->CheckDeadline();

    /**
     * @brief 异步读取并解析 HTTP 请求
     * @details
     * 这是一个组合操作 (Composed Operation):
     * 1. 调用 socket.read_some 读取字节到 _buffer。
     * 2. 调用 http parser 解析字节流。
     * 3. 如果 Header 不完整 (未找到 \r\n\r\n)，重复步骤 1。
     * 4. 如果 Body 不完整 (Content-Length 未满足)，重复步骤 1。
     * 5. 全部解析完成后，调用 lambda 回调。
     */
    http::async_read(
        _socket, _buffer, _request,
        [self](beast::error_code ec, std::size_t bytes_transferred)
        {
            try
            {
                if (ec)
                {
                    // EOF (End of File) 表示对端关闭了连接，是正常流程
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

void HttpConnection::PreParseGetParam()
{
    // 提取 URI
    auto uri = _request.target();
    // 查找查询字符串的开始位置（即 '?' 的位置）
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos)
    {
        _get_url = uri;
        return;
    }

    _get_url = uri.substr(0, query_pos);
    std::string query_string = uri.substr(query_pos + 1);
    std::string key;
    std::string value;
    size_t pos = 0;
    // 手动状态机解析 key=value&key2=value2
    while ((pos = query_string.find('&')) != std::string::npos)
    {
        auto pair = query_string.substr(0, pos);
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos)
        {
            key = UrlDecode(pair.substr(0, eq_pos));
            value = UrlDecode(pair.substr(eq_pos + 1));
            _get_params[key] = value;
        }
        query_string.erase(0, pos + 1);
    }
    // 处理最后一个参数对（如果没有 & 分隔符）
    if (!query_string.empty())
    {
        size_t eq_pos = query_string.find('=');
        if (eq_pos != std::string::npos)
        {
            key = UrlDecode(query_string.substr(0, eq_pos));
            value = UrlDecode(query_string.substr(eq_pos + 1));
            _get_params[key] = value;
        }
    }
}

void HttpConnection::HandleRequest()
{
    // [Protocol Compliance]
    // 设置 HTTP 版本 (1.0 或 1.1)
    _response.version(_request.version());
    // 设置 Keep-Alive 属性。如果是 1.1，默认是 true；如果是 1.0，取决于 Connection header。
    bool keep_alive = _request.keep_alive();
    _response.keep_alive(keep_alive);

    if (_request.method() == http::verb::get)
    {
        PreParseGetParam();
        // [Routing] 路由分发
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
        if (!success)
        {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url not found \r\n";
            WriteResponse();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }

    if (_request.method() == http::verb::post)
    {
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
        if (!success)
        {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "url not found\r\n";
            WriteResponse();
            return;
        }

        _response.result(http::status::ok);
        _response.set(http::field::server, "GateServer");
        WriteResponse();
        return;
    }
}

void HttpConnection::WriteResponse()
{
    auto self = shared_from_this();
    // 必须显式设置 Content-Length，否则客户端可能不知道响应何时结束
    _response.content_length(_response.body().size());

    /**
     * @brief 异步写回响应
     * @details 将用户态 buffer 的数据拷贝到内核态 socket 发送缓冲区。
     */
    http::async_write(
        _socket, _response,
        [self](beast::error_code ec, std::size_t)
        {
            if (ec)
            {
                // 发送失败，关闭连接
                self->_socket.shutdown(tcp::socket::shutdown_send, ec);
                std::cout << "socket shutdown" << std::endl;
                return;
            }

            // [Keep-Alive Handling]
            // 如果协议支持长连接，则清空 Request/Response 对象，递归调用 ReadRequest 等待下一个请求。
            // 否则，主动关闭连接。
            if (self->_response.keep_alive())
            {
                self->_request = {};
                self->_response = {};
                self->ReadRequest();
            }
            else
            {
                self->_socket.shutdown(tcp::socket::shutdown_send, ec);
                std::cout << "socket shutdown" << std::endl;
                self->deadline_.cancel();
            }
        });
}

void HttpConnection::CheckDeadline()
{
    auto self = shared_from_this();

    /**
     * @brief 异步等待定时器
     * @details
     * - 情况1: 超时。ec == 0。调用 close() 关闭 socket。
     * - 情况2: 定时器被取消 (cancel)。ec == operation_aborted。什么都不做。
     * - 情况3: 定时器重新设置 (expires_after)。也会触发 operation_aborted。
     */
    deadline_.async_wait(
        [self](beast::error_code ec)
        {
            if (!ec)
            {
                // 真正的超时发生了，硬关闭 Socket
                // 这会导致任何正在进行的 read/write 操作立即返回 error，从而中断连接
                std::cout << "socket close" << std::endl;
                self->_socket.close();
            }
        });
}
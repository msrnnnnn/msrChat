/**
 * @file    HttpConnection.cpp
 * @brief   HTTP 连接类实现
 * @author  msr
 */

#include "HttpConnection.h"
#include "LogicSystem.h"
#include <iostream>
#include <string>
#include <string_view>

HttpConnection::HttpConnection(tcp::socket &&socket)
    : _socket(std::move(socket))
{
}

void HttpConnection::Start()
{
    auto self = shared_from_this();

    // [Heartbeat] 更新超时时间：倒计时60秒(此时还没开始计时)
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
        [self](beast::error_code ec, std::size_t)
        {
            try
            {
                if (ec)
                {
                    // EOF (End of File) 表示对端关闭了连接，是正常流程
                    std::cout << "http read is" << ec.what() << std::endl;
                    return;
                }
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
    // 打印请求信息（调试用）
    std::cout << "[HTTP Request] Method: " << _request.method_string() << ", Target: " << _request.target()
              << std::endl;

    // [Protocol Compliance]
    // 设置 HTTP 版本 (1.0 或 1.1)
    _response.version(_request.version());
    // 设置 Keep-Alive 属性。如果是 1.1，默认是 true；如果是 1.0，取决于 Connection header。
    bool keep_alive = _request.keep_alive();
    _response.keep_alive(keep_alive);

    if (_request.method() == http::verb::get)
    {
        PreParseGetParam();
        std::cout << "[Routing] GET request to: " << _get_url << std::endl;
        // [Routing] 路由分发
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
        if (!success)
        {
            std::cout << "[Routing] Route not found: " << _get_url << std::endl;
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
        std::cout << "[Routing] POST request to: " << _request.target() << std::endl;
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
        if (!success)
        {
            std::cout << "[Routing] Route not found: " << _request.target() << std::endl;
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
                self->Start();
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
    // 1. 使用 string_view 避免拷贝
    std::string_view uri = _request.target(); // Beast 也是返回 string_view

    // 2. 查找 ?
    auto query_pos = uri.find('?');
    if (query_pos == std::string_view::npos)
    {
        _get_url = std::string(uri); // 这里必须拷贝，因为 _get_url 拥有所有权
        return;
    }

    _get_url = std::string(uri.substr(0, query_pos));

    // 获取参数部分视图
    std::string_view query_string = uri.substr(query_pos + 1);

    // 3. 零拷贝切割 loop
    while (!query_string.empty())
    {
        // 找 &
        auto amp_pos = query_string.find('&');

        // 拿到当前的 kv 段 (例如 "id=1")
        // 如果找不到 &，说明是最后一段
        auto segment = (amp_pos == std::string_view::npos) ? query_string : query_string.substr(0, amp_pos);

        // 找 =
        auto eq_pos = segment.find('=');
        if (eq_pos != std::string_view::npos)
        {
            // 拿到 key 和 value 的视图 (还在原始 buffer 里，没发生拷贝)
            auto key_view = segment.substr(0, eq_pos);
            auto val_view = segment.substr(eq_pos + 1);

            // 只有在最后存入 Map 且需要解码时，才真正发生拷贝和内存分配
            // 假设 UrlDecode 接收 string_view 返回 string
            _get_params[UrlDecode(std::string(key_view))] = UrlDecode(std::string(val_view));
        }

        // 移动视图窗口，而不是移动内存 (erase)
        if (amp_pos == std::string_view::npos)
        {
            break;
        }
        else
        {
            // 指针向后跳，O(1) 操作
            query_string.remove_prefix(amp_pos + 1);
        }
    }
}

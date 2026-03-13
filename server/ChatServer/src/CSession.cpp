#include "CSession.h"
#include "CServer.h"
#include "const.h"
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

CSession::CSession(boost::asio::io_context &ioc, CServer *server)
    : _socket(ioc),
      _read_deadline(ioc),
      _server(server)
{
    _uuid = std::to_string(CServer::s_session_id_allocator.fetch_add(1));
    _recv_head_node = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, 0);
}

CSession::~CSession()
{
    spdlog::info("~CSession: {}", _uuid);
}

void CSession::Close()
{
    bool expected = false;
    if (!_b_closed.compare_exchange_strong(expected, true))
    {
        return;
    }
    if (_user_uid != 0)
    {
        _server->RemoveUserSession(_user_uid);
        _user_uid = 0;
    }
    boost::system::error_code ec;
    _read_deadline.cancel(ec);
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    _socket.close(ec);
}

void CSession::Start()
{
    ResetReadDeadline();
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::ResetReadDeadline()
{
    _read_deadline.expires_after(std::chrono::seconds(60));
    auto self = shared_from_this();
    _read_deadline.async_wait(
        [this, self](const boost::system::error_code &ec)
        {
            if (ec)
            {
                return;
            }
            Close();
            _server->ClearSession(_uuid);
        });
}

void CSession::AsyncReadHead(int total_len)
{
    auto self = shared_from_this();
    boost::asio::async_read(
        _socket, boost::asio::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
        [this, self](const boost::system::error_code &ec, std::size_t bytes)
        {
            if (ec)
            {
                // 如果已登录，从用户会话映射中移除
                if (_user_uid != 0)
                {
                    _server->RemoveUserSession(_user_uid);
                    _user_uid = 0;
                }
                Close();
                _server->ClearSession(_uuid);
                return;
            }
            ResetReadDeadline();
            uint16_t msg_id = 0;
            uint32_t msg_len = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_long(msg_len);

            if (msg_len == 0)
            {
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            if (msg_len > MAX_LENGTH)
            {
                _server->ClearSession(_uuid);
                return;
            }
            _recv_msg_node = std::make_shared<RecvNode>(msg_len, msg_id);
            AsyncReadBody(static_cast<int>(msg_len));
        });
}

void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    boost::asio::async_read(
        _socket, boost::asio::buffer(_recv_msg_node->_data, total_len),
        [this, self, total_len](const boost::system::error_code &ec, std::size_t bytes)
        {
            if (ec)
            {
                // 如果已登录，从用户会话映射中移除
                if (_user_uid != 0)
                {
                    _server->RemoveUserSession(_user_uid);
                    _user_uid = 0;
                }
                Close();
                _server->ClearSession(_uuid);
                return;
            }
            ResetReadDeadline();
            _recv_msg_node->_data[total_len] = '\0';
            spdlog::info("[Recv] ID: {} Data: {}", _recv_msg_node->_msg_id, _recv_msg_node->_data);

            // 处理消息
            uint16_t msg_id = _recv_msg_node->_msg_id;
            std::string body_data(_recv_msg_node->_data, total_len);

            try
            {
                if (msg_id == MSG_CHAT_LOGIN)
                {
                    auto json_data = nlohmann::json::parse(body_data);
                    int uid = json_data.value("uid", 0);
                    std::string token = json_data.value("token", "");

                    spdlog::info("[CSession] Login request - uid: {}, token: {}", uid, token);

                    nlohmann::json response;
                    if (uid <= 0 || token.empty())
                    {
                        response["error"] = 1;
                        response["message"] = "invalid login";
                        response["uid"] = uid;
                        Send(response.dump(), MSG_CHAT_LOGIN);
                    }
                    else if (_user_uid != 0)
                    {
                        response["error"] = 1;
                        response["message"] = "already login";
                        response["uid"] = _user_uid;
                        Send(response.dump(), MSG_CHAT_LOGIN);
                    }
                    else if (!_server->ValidateToken(uid, token))
                    {
                        spdlog::warn("[CSession] Token invalid for uid {}", uid);
                        response["error"] = 1;
                        response["message"] = "token invalid";
                        response["uid"] = uid;
                        Send(response.dump(), MSG_CHAT_LOGIN);
                    }
                    else
                    {
                        _server->AddUserSession(uid, shared_from_this());
                        _user_uid = uid;
                        response["error"] = 0;
                        response["message"] = "login success";
                        response["uid"] = uid;
                        Send(response.dump(), MSG_CHAT_LOGIN);
                        _server->SendOfflineMessages(uid, shared_from_this());
                    }
                }
                else if (msg_id == MSG_CHAT_TEXT)
                {
                    auto json_data = nlohmann::json::parse(body_data);
                    int from_uid = json_data.value("from_uid", 0);
                    int to_uid = json_data.value("to_uid", 0);
                    std::string content = json_data.value("content", "");
                    std::string client_msg_id = json_data.value("client_msg_id", "");

                    if (_user_uid == 0)
                    {
                        nlohmann::json ack;
                        ack["error"] = 1;
                        ack["message"] = "not login";
                        ack["to_uid"] = to_uid;
                        ack["client_msg_id"] = client_msg_id;
                        Send(ack.dump(), MSG_CHAT_ACK);
                        AsyncReadHead(HEAD_TOTAL_LEN);
                        return;
                    }
                    if (from_uid != 0 && from_uid != _user_uid)
                    {
                        spdlog::warn("[CSession] from_uid mismatch client: {} server: {}", from_uid, _user_uid);
                    }
                    if (to_uid <= 0 || content.empty() || content.size() > MAX_CHAT_CONTENT_LEN)
                    {
                        nlohmann::json ack;
                        ack["error"] = 1;
                        ack["message"] = "invalid message";
                        ack["to_uid"] = to_uid;
                        ack["client_msg_id"] = client_msg_id;
                        Send(ack.dump(), MSG_CHAT_ACK);
                        AsyncReadHead(HEAD_TOTAL_LEN);
                        return;
                    }

                    nlohmann::json forward;
                    forward["from_uid"] = _user_uid;
                    forward["to_uid"] = to_uid;
                    forward["content"] = content;
                    if (!client_msg_id.empty())
                    {
                        forward["client_msg_id"] = client_msg_id;
                    }

                    bool delivered = _server->ForwardMessage(to_uid, forward.dump());
                    if (!delivered)
                    {
                        _server->StoreOfflineMessage(to_uid, forward.dump());
                    }

                    nlohmann::json ack;
                    ack["error"] = delivered ? 0 : 1;
                    ack["message"] = delivered ? "delivered" : "stored";
                    ack["to_uid"] = to_uid;
                    ack["client_msg_id"] = client_msg_id;
                    Send(ack.dump(), MSG_CHAT_ACK);
                }
                else
                {
                    // 其他消息类型，原样回显 (Echo)
                    Send(body_data, msg_id);
                }
            }
            catch (const std::exception &e)
            {
                spdlog::error("[CSession] JSON parse error: {}", e.what());
                nlohmann::json error_response;
                error_response["error"] = 1;
                error_response["message"] = std::string("parse error: ") + e.what();
                Send(error_response.dump(), msg_id);
            }

            AsyncReadHead(HEAD_TOTAL_LEN);
        });
}

void CSession::Send(const std::string &msg, short msg_id)
{
    auto send_node = std::make_shared<SendNode>(msg, static_cast<uint16_t>(msg_id));
    auto self = shared_from_this();
    boost::asio::post(
        _socket.get_executor(),
        [this, self, send_node]()
        {
            bool need_write = false;
            {
                std::lock_guard<std::mutex> lock(_send_mtx);
                _send_queue.push(send_node);
                if (!_is_writing)
                {
                    _is_writing = true;
                    need_write = true;
                }
            }
            if (need_write)
            {
                AsyncWriteMsg();
            }
        });
}

void CSession::AsyncWriteMsg()
{
    // 在锁内获取队首元素拷贝
    std::shared_ptr<SendNode> send_node;
    {
        std::lock_guard<std::mutex> lock(_send_mtx);
        if (_send_queue.empty())
        {
            _is_writing = false;
            return;
        }
        send_node = _send_queue.front();
    }

    // 释放锁后执行异步写操作
    auto self = shared_from_this();
    boost::asio::async_write(
        _socket, boost::asio::buffer(send_node->_data, send_node->_total_len + 6),
        [this, self](const boost::system::error_code &ec, std::size_t bytes)
        {
            if (ec)
            {
                // 如果已登录，从用户会话映射中移除
                if (_user_uid != 0)
                {
                    _server->RemoveUserSession(_user_uid);
                    _user_uid = 0;
                }
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            // 在锁内执行 pop 操作
            bool has_more = false;
            {
                std::lock_guard<std::mutex> lock(_send_mtx);
                _send_queue.pop();
                if (_send_queue.empty())
                {
                    _is_writing = false;
                    return;
                }
                has_more = true;
            }

            // 如果队列不为空，在锁外递归调用 AsyncWriteMsg
            if (has_more)
            {
                AsyncWriteMsg();
            }
        });
}

/**
 * @file CSession.cpp
 * @brief TCP 会话实现
 * @details 负责协议解析、登录鉴权、消息转发与离线消息处理。
 */
#include "CSession.h"
#include "CServer.h"
#include "SQLiteMgr.h"
#include "const.h"
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

/**
 * @brief 构造函数
 * @param ioc io_context 引用
 * @param server 所属服务器指针
 */
CSession::CSession(boost::asio::io_context &ioc, CServer *server)
    : _socket(ioc),
      _read_deadline(ioc),
      _server(server)
{
    _uuid = std::to_string(CServer::s_session_id_allocator.fetch_add(1));
    _recv_head_node = std::make_shared<RecvNode>();
    _recv_head_node->Reset(HEAD_TOTAL_LEN, 0);
    _recv_msg_node = std::make_shared<RecvNode>();
}

/**
 * @brief 析构函数
 */
CSession::~CSession()
{
    spdlog::info("~CSession: {}", _uuid);
}

/**
 * @brief 关闭连接并清理会话状态
 */
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

/**
 * @brief 启动读循环
 */
void CSession::Start()
{
    ResetReadDeadline();
    AsyncReadHead(HEAD_TOTAL_LEN);
}

/**
 * @brief 重置读超时定时器
 */
void CSession::ResetReadDeadline()
{
    _read_deadline.expires_after(std::chrono::seconds(300));
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

/**
 * @brief 异步读取消息头
 * @param total_len 头部长度
 */
void CSession::AsyncReadHead(int total_len)
{
    auto self = shared_from_this();
    auto head_node = _recv_head_node;
    boost::asio::async_read(
        _socket, boost::asio::buffer(head_node->_data, HEAD_TOTAL_LEN),
        [this, self, head_node](const boost::system::error_code &ec, std::size_t bytes)
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
            memcpy(&msg_id, head_node->_data, HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            memcpy(&msg_len, head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
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
            _recv_msg_node->Reset(msg_len, msg_id);
            AsyncReadBody(static_cast<int>(msg_len));
        });
}

/**
 * @brief 异步读取消息体
 * @param total_len 消息体长度
 */
void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    auto recv_msg_node = _recv_msg_node;
    boost::asio::async_read(
        _socket, boost::asio::buffer(recv_msg_node->_data, total_len),
        [this, self, recv_msg_node, total_len](const boost::system::error_code &ec, std::size_t bytes)
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
            recv_msg_node->_data[total_len] = '\0';
            // spdlog::info("[Recv] ID: {} Data: {}", recv_msg_node->_msg_id, recv_msg_node->_data);

            uint16_t msg_id = recv_msg_node->_msg_id;
            std::string body_data(recv_msg_node->_data, total_len);
            bool continue_read = true;

            try
            {
                if (msg_id == MSG_CHAT_LOGIN)
                {
                    continue_read = false;
                    HandleLoginRequest(body_data);
                }
                else if (msg_id == ID_REGISTER_USER)
                {
                    continue_read = false;
                    HandleRegisterRequest(body_data);
                }
                else if (msg_id == ID_LOGIN_USER)
                {
                    continue_read = false;
                    HandleLoginAuthRequest(body_data);
                }
                else if (msg_id == ID_GET_VARIFY_CODE)
                {
                    continue_read = false;
                    HandleGetVerifyCodeRequest(body_data);
                }
                else if (msg_id == ID_RESET_PWD)
                {
                    continue_read = false;
                    HandleResetPwdRequest(body_data);
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

                    std::string forward_data = forward.dump();
                    bool delivered = _server->ForwardMessage(to_uid, forward_data);
                    if (!delivered)
                    {
                        _server->StoreOfflineMessage(to_uid, forward_data);
                    }

                    // 回复发送结果
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

            if (continue_read)
            {
                AsyncReadHead(HEAD_TOTAL_LEN);
            }
        });
}

void CSession::HandleLoginRequest(const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data, nullptr, false);
    nlohmann::json response;

    if (json_data.is_discarded())
    {
        response["error"] = 1;
        response["message"] = "invalid login payload";
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    int uid = json_data.value("uid", 0);
    std::string token = json_data.value("token", "");

    if (uid <= 0 || token.empty())
    {
        response["error"] = 1;
        response["message"] = "invalid login";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    if (_user_uid != 0)
    {
        response["error"] = 1;
        response["message"] = "already login";
        response["uid"] = _user_uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    bool expected = false;
    if (!_login_in_progress.compare_exchange_strong(expected, true))
    {
        response["error"] = 1;
        response["message"] = "login in progress";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    OnLoginValidated(uid, _server->CheckToken(uid, token));
}

void CSession::OnLoginValidated(int uid, bool valid)
{
    _login_in_progress.store(false);

    if (_b_closed.load())
    {
        return;
    }

    nlohmann::json response;
    if (!valid)
    {
        spdlog::warn("[CSession] Token invalid for uid {}", uid);
        response["error"] = 1;
        response["message"] = "token invalid";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    if (_user_uid != 0)
    {
        response["error"] = 1;
        response["message"] = "already login";
        response["uid"] = _user_uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    _server->AddUserSession(uid, shared_from_this());
    _user_uid = uid;

    response["error"] = 0;
    response["message"] = "login success";
    response["uid"] = uid;
    Send(response.dump(), MSG_CHAT_LOGIN);
    _server->SendOfflineMessages(uid, shared_from_this());
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::HandleRegisterRequest(const std::string &body_data)
{
    nlohmann::json response;
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string password_hash = json_data.value("passwd", "");
        std::string email = json_data.value("email", "");

        if (username.empty() || password_hash.empty() || email.empty())
        {
            response["error"] = 1;
            Send(response.dump(), ID_REGISTER_USER);
            AsyncReadHead(HEAD_TOTAL_LEN);
            return;
        }

        AuthResult result = SQLiteMgr::Instance().RegisterUser(username, password_hash, email);
        response["error"] = result.error;
        if (result.error == 0)
        {
            response["uid"] = result.uid;
            response["username"] = result.username;
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] Register error: {}", e.what());
        response["error"] = 1;
    }

    Send(response.dump(), ID_REGISTER_USER);
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::HandleLoginAuthRequest(const std::string &body_data)
{
    nlohmann::json response;
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int uid = json_data.value("uid", 0);
        std::string token = json_data.value("token", "");

        if (uid <= 0 || token.empty())
        {
            response["error"] = 1;
            response["message"] = "invalid parameters";
            Send(response.dump(), MSG_CHAT_LOGIN);
            AsyncReadHead(HEAD_TOTAL_LEN);
            return;
        }

        bool valid = _server->CheckToken(uid, token);
        if (!valid)
        {
            response["error"] = 1;
            response["message"] = "token invalid or expired";
            Send(response.dump(), MSG_CHAT_LOGIN);
            AsyncReadHead(HEAD_TOTAL_LEN);
            return;
        }

        _user_uid = uid;
        response["error"] = 0;
        response["message"] = "login success";
        response["uid"] = uid;
        spdlog::info("[CSession] User {} chat login success", uid);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] LoginAuth error: {}", e.what());
        response["error"] = 1;
        response["message"] = "server error";
    }

    Send(response.dump(), MSG_CHAT_LOGIN);
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::HandleGetVerifyCodeRequest(const std::string &body_data)
{
    nlohmann::json response;
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string email = json_data.value("email", "");

        if (email.empty())
        {
            response["error"] = 1;
            Send(response.dump(), ID_GET_VARIFY_CODE);
            AsyncReadHead(HEAD_TOTAL_LEN);
            return;
        }

        bool success = SQLiteMgr::Instance().SendVerifyCode(email);
        response["error"] = success ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] GetVerifyCode error: {}", e.what());
        response["error"] = 1;
    }

    Send(response.dump(), ID_GET_VARIFY_CODE);
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::HandleResetPwdRequest(const std::string &body_data)
{
    nlohmann::json response;
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string email = json_data.value("email", "");
        std::string code = json_data.value("varifycode", "");
        std::string new_password_hash = json_data.value("passwd", "");

        if (username.empty() || email.empty() || code.empty() || new_password_hash.empty())
        {
            response["error"] = 1;
            Send(response.dump(), ID_RESET_PWD);
            AsyncReadHead(HEAD_TOTAL_LEN);
            return;
        }

        int verify_result = SQLiteMgr::Instance().CheckVerifyCode(email, code);
        if (verify_result != 0)
        {
            response["error"] = verify_result;
            Send(response.dump(), ID_RESET_PWD);
            AsyncReadHead(HEAD_TOTAL_LEN);
            return;
        }

        bool success = SQLiteMgr::Instance().ResetPassword(username, email, code, new_password_hash);
        response["error"] = success ? 0 : 1009;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] ResetPwd error: {}", e.what());
        response["error"] = 1;
    }

    Send(response.dump(), ID_RESET_PWD);
    AsyncReadHead(HEAD_TOTAL_LEN);
}

/**
 * @brief 发送消息
 * @param msg 消息体
 * @param msg_id 消息类型
 */
void CSession::Send(const std::string &msg, short msg_id)
{
    auto send_node = std::make_shared<SendNode>();
    send_node->Reset(msg, static_cast<uint16_t>(msg_id));
    auto self = shared_from_this();
    boost::asio::dispatch(
        _socket.get_executor(),
        [this, self, send_node]()
        {
            _send_queue.push_back(send_node);
            if (_is_writing)
            {
                return;
            }
            _is_writing = true;
            AsyncWriteMsg();
        });
}

/**
 * @brief 异步发送队列中的消息
 */
void CSession::AsyncWriteMsg()
{
    if (_send_queue.empty())
    {
        _is_writing = false;
        return;
    }
    auto send_node = _send_queue.front();
    auto self = shared_from_this();
    boost::asio::async_write(
        _socket, boost::asio::buffer(send_node->_data, send_node->_total_len + 6),
        [this, self, send_node](const boost::system::error_code &ec, std::size_t bytes)
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

            _send_queue.pop_front();
            if (_send_queue.empty())
            {
                _is_writing = false;
                return;
            }
            AsyncWriteMsg();
        });
}

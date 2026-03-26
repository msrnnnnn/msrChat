/**
 * @file CSession.cpp
 * @brief TCP 会话实现
 * @details 负责协议解析、登录鉴权、消息转发与离线消息处理。
 */
#include "CSession.h"
#include "CServer.h"
#include "const.h"
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>

namespace
{
void AppendEscapedJsonString(std::string &output, std::string_view value)
{
    output.push_back('"');
    for (unsigned char ch : value)
    {
        switch (ch)
        {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (ch < 0x20)
                {
                    static constexpr char kHexDigits[] = "0123456789abcdef";
                    output += "\\u00";
                    output.push_back(kHexDigits[(ch >> 4) & 0x0F]);
                    output.push_back(kHexDigits[ch & 0x0F]);
                }
                else
                {
                    output.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    output.push_back('"');
}

void AppendJsonFieldName(std::string &output, std::string_view key)
{
    AppendEscapedJsonString(output, key);
    output.push_back(':');
}

void AppendJsonStringField(std::string &output, std::string_view key, std::string_view value, bool with_comma)
{
    if (with_comma)
    {
        output.push_back(',');
    }
    AppendJsonFieldName(output, key);
    AppendEscapedJsonString(output, value);
}

void AppendJsonIntField(std::string &output, std::string_view key, int value, bool with_comma)
{
    if (with_comma)
    {
        output.push_back(',');
    }
    AppendJsonFieldName(output, key);
    output += std::to_string(value);
}

std::string BuildAckResponse(int error, std::string_view message, int to_uid, std::string_view client_msg_id)
{
    std::string output;
    output.reserve(96 + client_msg_id.size());
    output.push_back('{');
    AppendJsonIntField(output, "error", error, false);
    AppendJsonStringField(output, "message", message, true);
    AppendJsonIntField(output, "to_uid", to_uid, true);
    AppendJsonStringField(output, "client_msg_id", client_msg_id, true);
    output.push_back('}');
    return output;
}

std::string BuildLoginResponse(int error, std::string_view message, int uid)
{
    std::string output;
    output.reserve(64);
    output.push_back('{');
    AppendJsonIntField(output, "error", error, false);
    AppendJsonStringField(output, "message", message, true);
    AppendJsonIntField(output, "uid", uid, true);
    output.push_back('}');
    return output;
}

std::string BuildForwardMessage(int from_uid, int to_uid, std::string_view content, std::string_view client_msg_id)
{
    std::string output;
    output.reserve(96 + content.size() + client_msg_id.size());
    output.push_back('{');
    AppendJsonIntField(output, "from_uid", from_uid, false);
    AppendJsonIntField(output, "to_uid", to_uid, true);
    AppendJsonStringField(output, "content", content, true);
    if (!client_msg_id.empty())
    {
        AppendJsonStringField(output, "client_msg_id", client_msg_id, true);
    }
    output.push_back('}');
    return output;
}

const std::string *GetOptionalStringRef(const nlohmann::json &json_data, const char *key)
{
    auto it = json_data.find(key);
    if (it == json_data.end() || !it->is_string())
    {
        return nullptr;
    }
    return &it->get_ref<const std::string &>();
}
} // namespace

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
            bool continue_read = true;

            try
            {
                if (msg_id == MSG_CHAT_LOGIN)
                {
                    continue_read = false;
                    HandleLoginRequest(std::string_view(recv_msg_node->_data, static_cast<std::size_t>(total_len)));
                }
                else if (msg_id == MSG_CHAT_TEXT)
                {
                    auto json_data = nlohmann::json::parse(recv_msg_node->_data, recv_msg_node->_data + total_len);
                    int from_uid = json_data.value("from_uid", 0);
                    int to_uid = json_data.value("to_uid", 0);
                    const std::string *content = GetOptionalStringRef(json_data, "content");
                    const std::string *client_msg_id = GetOptionalStringRef(json_data, "client_msg_id");
                    const std::string_view client_msg_id_view =
                        client_msg_id ? std::string_view(*client_msg_id) : std::string_view();

                    if (_user_uid == 0)
                    {
                        Send(BuildAckResponse(1, "not login", to_uid, client_msg_id_view), MSG_CHAT_ACK);
                        AsyncReadHead(HEAD_TOTAL_LEN);
                        return;
                    }
                    if (from_uid != 0 && from_uid != _user_uid)
                    {
                        spdlog::warn("[CSession] from_uid mismatch client: {} server: {}", from_uid, _user_uid);
                    }
                    if (to_uid <= 0 || content == nullptr || content->empty() || content->size() > MAX_CHAT_CONTENT_LEN)
                    {
                        Send(BuildAckResponse(1, "invalid message", to_uid, client_msg_id_view), MSG_CHAT_ACK);
                        AsyncReadHead(HEAD_TOTAL_LEN);
                        return;
                    }

                    std::string forward_data = BuildForwardMessage(_user_uid, to_uid, *content, client_msg_id_view);
                    bool delivered = _server->ForwardMessage(to_uid, forward_data);
                    if (!delivered)
                    {
                        _server->StoreOfflineMessage(to_uid, forward_data);
                    }

                    Send(
                        BuildAckResponse(delivered ? 0 : 1, delivered ? "delivered" : "stored", to_uid, client_msg_id_view),
                        MSG_CHAT_ACK);
                }
                else
                {
                    Send(std::string(recv_msg_node->_data, static_cast<std::size_t>(total_len)), msg_id);
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

void CSession::HandleLoginRequest(std::string_view body_data)
{
    auto json_data = nlohmann::json::parse(body_data.begin(), body_data.end(), nullptr, false);

    if (json_data.is_discarded())
    {
        Send(BuildLoginResponse(1, "invalid login payload", 0), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    int uid = json_data.value("uid", 0);
    const std::string *token = GetOptionalStringRef(json_data, "token");

    if (uid <= 0 || token == nullptr || token->empty())
    {
        Send(BuildLoginResponse(1, "invalid login", uid), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    if (_user_uid != 0)
    {
        Send(BuildLoginResponse(1, "already login", _user_uid), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    bool expected = false;
    if (!_login_in_progress.compare_exchange_strong(expected, true))
    {
        Send(BuildLoginResponse(1, "login in progress", uid), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    auto weak_self = std::weak_ptr<CSession>(shared_from_this());
    _server->ValidateTokenAsync(
        _socket.get_executor(), uid, *token,
        [weak_self, uid](bool valid)
        {
            auto self = weak_self.lock();
            if (!self)
            {
                return;
            }
            self->OnLoginValidated(uid, valid);
        });
}

void CSession::OnLoginValidated(int uid, bool valid)
{
    _login_in_progress.store(false);

    if (_b_closed.load())
    {
        return;
    }

    if (!valid)
    {
        spdlog::warn("[CSession] Token invalid for uid {}", uid);
        Send(BuildLoginResponse(1, "token invalid", uid), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    if (_user_uid != 0)
    {
        Send(BuildLoginResponse(1, "already login", _user_uid), MSG_CHAT_LOGIN);
        AsyncReadHead(HEAD_TOTAL_LEN);
        return;
    }

    _server->AddUserSession(uid, shared_from_this());
    _user_uid = uid;

    Send(BuildLoginResponse(0, "login success", uid), MSG_CHAT_LOGIN);
    _server->SendOfflineMessages(uid, shared_from_this());
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

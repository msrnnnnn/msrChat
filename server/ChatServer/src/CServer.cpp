/**
 * @file CServer.cpp
 * @brief 聊天服务 TCP 入口实现
 */
#include "CServer.h"
#include "AsioIOServicePool.h"
#include "CSession.h"
#include "RedisMgr.h"
#include "const.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace
{
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class TokenValidationRequest : public std::enable_shared_from_this<TokenValidationRequest>
{
public:
    TokenValidationRequest(
        const net::any_io_executor &executor, std::string host, std::string port, int uid, std::string token,
        TokenValidationHandler handler)
        : resolver_(executor),
          stream_(executor),
          host_(std::move(host)),
          port_(std::move(port)),
          handler_(std::move(handler))
    {
        nlohmann::json req_json;
        req_json["uid"] = uid;
        req_json["token"] = std::move(token);

        request_.version(11);
        request_.method(http::verb::post);
        request_.target("/verify_token");
        request_.set(http::field::host, host_);
        request_.set(http::field::content_type, "application/json");
        request_.body() = req_json.dump();
        request_.prepare_payload();
    }

    void Start()
    {
        resolver_.async_resolve(
            host_, port_,
            [self = shared_from_this()](const boost::system::error_code &ec, const tcp::resolver::results_type &results)
            {
                self->OnResolve(ec, results);
            });
    }

private:
    void OnResolve(const boost::system::error_code &ec, const tcp::resolver::results_type &results)
    {
        if (ec)
        {
            spdlog::warn("[CServer] ValidateToken resolve failed: {}", ec.message());
            Complete(false);
            return;
        }

        stream_.expires_after(std::chrono::seconds(5));
        stream_.async_connect(
            results,
            [self = shared_from_this()](
                const boost::system::error_code &connect_ec, const tcp::resolver::results_type::endpoint_type &)
            {
                self->OnConnect(connect_ec);
            });
    }

    void OnConnect(const boost::system::error_code &ec)
    {
        if (ec)
        {
            spdlog::warn("[CServer] ValidateToken connect failed: {}", ec.message());
            Complete(false);
            return;
        }

        stream_.expires_after(std::chrono::seconds(5));
        http::async_write(
            stream_, request_,
            [self = shared_from_this()](const boost::system::error_code &write_ec, std::size_t)
            {
                self->OnWrite(write_ec);
            });
    }

    void OnWrite(const boost::system::error_code &ec)
    {
        if (ec)
        {
            spdlog::warn("[CServer] ValidateToken write failed: {}", ec.message());
            Complete(false);
            return;
        }

        stream_.expires_after(std::chrono::seconds(5));
        http::async_read(
            stream_, buffer_, response_,
            [self = shared_from_this()](const boost::system::error_code &read_ec, std::size_t)
            {
                self->OnRead(read_ec);
            });
    }

    void OnRead(const boost::system::error_code &ec)
    {
        if (ec)
        {
            spdlog::warn("[CServer] ValidateToken read failed: {}", ec.message());
            Complete(false);
            return;
        }

        auto resp_json = nlohmann::json::parse(response_.body(), nullptr, false);
        if (!resp_json.is_object())
        {
            Complete(false);
            return;
        }

        Complete(resp_json.value("error", 1) == 0);
    }

    void Complete(bool valid)
    {
        if (completed_)
        {
            return;
        }
        completed_ = true;

        beast::error_code ec;
        if (stream_.socket().is_open())
        {
            stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
            stream_.socket().close(ec);
        }

        if (handler_)
        {
            auto handler = std::move(handler_);
            handler(valid);
        }
    }

    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    std::string host_;
    std::string port_;
    TokenValidationHandler handler_;
    bool completed_ = false;
};
} // namespace

CServer::CServer(boost::asio::io_context &io_context, short port)
    : _io_context(io_context),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    spdlog::info("[CServer] Server initialized on port {}", port);
}

void CServer::Start()
{
    DoAccept();
}

/**
 * @brief 持续异步接收新连接
 */
void CServer::DoAccept()
{
    auto &pool = AsioIOServicePool::getInstance();
    auto new_session = std::make_shared<CSession>(pool.GetIOService(), this);
    _acceptor.async_accept(
        new_session->GetSocket(),
        [this, new_session](const boost::system::error_code &ec)
        {
            if (!ec)
            {
                spdlog::info("[CServer] New connection accepted: {}", new_session->GetUuid());
                _uuid_sessions.Insert(new_session->GetUuid(), new_session);
                new_session->Start();
            }
            else
            {
                spdlog::error("[CServer] Accept error: {}", ec.message());
            }
            DoAccept();
        });
}

void CServer::AddUserSession(int uid, std::shared_ptr<CSession> session)
{
    _uid_sessions.Insert(uid, std::move(session));
    spdlog::info("[CServer] User {} session added.", uid);
}

void CServer::RemoveUserSession(int uid)
{
    if (_uid_sessions.Erase(uid))
    {
        spdlog::info("[CServer] User {} session removed.", uid);
    }
}

void CServer::ClearSession(const std::string &uuid)
{
    if (_uuid_sessions.Erase(uuid))
    {
        spdlog::info("[CServer] Session {} cleared.", uuid);
    }
}

bool CServer::ForwardMessage(int target_uid, const std::string &msg_data)
{
    auto target_session = _uid_sessions.Find(target_uid).value_or(nullptr);
    if (!target_session)
    {
        return false;
    }

    target_session->Send(msg_data, MSG_CHAT_TEXT);
    // spdlog::info("[CServer] Message forwarded to user {}", target_uid);
    return true;
}

void CServer::StoreOfflineMessage(int target_uid, const std::string &msg_data)
{
    std::string key = "offline_msg:" + std::to_string(target_uid);
    RedisMgr::GetInstance()->LPush(key, msg_data);
}

/**
 * @brief 将 Redis 中离线消息逐条转发给当前会话
 */
void CServer::SendOfflineMessages(int uid, std::shared_ptr<CSession> session)
{
    std::string key = "offline_msg:" + std::to_string(uid);
    std::string msg;
    while (RedisMgr::GetInstance()->LPop(key, msg))
    {
        if (!msg.empty())
        {
            session->Send(msg, MSG_CHAT_TEXT);
        }
    }
}

void CServer::ValidateTokenAsync(
    const boost::asio::any_io_executor &executor, int uid, const std::string &token, TokenValidationHandler handler)
{
    if (_auth_host.empty() || _auth_port.empty())
    {
        boost::asio::post(executor, [handler = std::move(handler)]() mutable {
            if (handler)
            {
                handler(false);
            }
        });
        return;
    }

    std::make_shared<TokenValidationRequest>(executor, _auth_host, _auth_port, uid, token, std::move(handler))->Start();
}

void CServer::SetAuthServer(const std::string &host, const std::string &port)
{
    _auth_host = host;
    _auth_port = port;
}

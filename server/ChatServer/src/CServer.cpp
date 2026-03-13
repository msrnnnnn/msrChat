#include "CServer.h"
#include "AsioIOServicePool.h"
#include "CSession.h"
#include "RedisMgr.h"
#include "const.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

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
                {
                    std::lock_guard<std::mutex> lock(_uuid_session_mtx);
                    _uuid_sessions[new_session->GetUuid()] = new_session;
                }
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
    std::lock_guard<std::mutex> lock(_session_mtx);
    _uid_sessions[uid] = session;
    spdlog::info("[CServer] User {} session added.", uid);
}

void CServer::RemoveUserSession(int uid)
{
    std::lock_guard<std::mutex> lock(_session_mtx);
    auto it = _uid_sessions.find(uid);
    if (it != _uid_sessions.end())
    {
        _uid_sessions.erase(it);
        spdlog::info("[CServer] User {} session removed.", uid);
    }
}

void CServer::ClearSession(const std::string &uuid)
{
    std::lock_guard<std::mutex> lock(_uuid_session_mtx);
    auto it = _uuid_sessions.find(uuid);
    if (it != _uuid_sessions.end())
    {
        _uuid_sessions.erase(it);
        spdlog::info("[CServer] Session {} cleared.", uuid);
    }
}

bool CServer::ForwardMessage(int target_uid, const std::string &msg_data)
{
    std::shared_ptr<CSession> target_session;
    {
        std::lock_guard<std::mutex> lock(_session_mtx);
        auto it = _uid_sessions.find(target_uid);
        if (it != _uid_sessions.end())
        {
            target_session = it->second;
        }
    }

    if (!target_session)
    {
        return false;
    }

    target_session->Send(msg_data, MSG_CHAT_TEXT);
    spdlog::info("[CServer] Message forwarded to user {}", target_uid);
    return true;
}

void CServer::StoreOfflineMessage(int target_uid, const std::string &msg_data)
{
    std::string key = "offline_msg:" + std::to_string(target_uid);
    RedisMgr::GetInstance()->LPush(key, msg_data);
}

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

bool CServer::ValidateToken(int uid, const std::string &token)
{
    if (_auth_host.empty() || _auth_port.empty())
    {
        return false;
    }

    try
    {
        namespace beast = boost::beast;
        namespace http = beast::http;
        namespace net = boost::asio;
        using tcp = net::ip::tcp;

        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(_auth_host, _auth_port);

        beast::tcp_stream stream(ioc);
        stream.connect(results);

        nlohmann::json req_json;
        req_json["uid"] = uid;
        req_json["token"] = token;

        http::request<http::string_body> req{http::verb::post, "/verify_token", 11};
        req.set(http::field::host, _auth_host);
        req.set(http::field::content_type, "application/json");
        req.body() = req_json.dump();
        req.prepare_payload();

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        auto resp_json = nlohmann::json::parse(res.body(), nullptr, false);
        if (!resp_json.is_object())
        {
            return false;
        }
        int error = resp_json.value("error", 1);
        return error == 0;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CServer] ValidateToken exception: {}", e.what());
        return false;
    }
}

void CServer::SetAuthServer(const std::string &host, const std::string &port)
{
    _auth_host = host;
    _auth_port = port;
}

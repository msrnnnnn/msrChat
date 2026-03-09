#include "CServer.h"
#include "CSession.h"
#include "const.h"
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
    auto new_session = std::make_shared<CSession>(_io_context, this);
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

void CServer::ForwardMessage(int target_uid, const std::string &msg_data)
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

    if (target_session)
    {
        target_session->Send(msg_data, MSG_CHAT_TEXT);
        spdlog::info("[CServer] Message forwarded to user {}", target_uid);
    }
    else
    {
        spdlog::warn("[CServer] User {} not found, message dropped.", target_uid);
    }
}

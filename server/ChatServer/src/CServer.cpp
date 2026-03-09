#include "CServer.h"
#include "CSession.h"
#include "const.h"
#include <iostream>
#include <string>

CServer::CServer(boost::asio::io_context &io_context, short port)
    : _io_context(io_context),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    std::cout << "[CServer] Server initialized on port " << port << std::endl;
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
                std::cout << "[CServer] New connection accepted: " << new_session->GetUuid() << std::endl;
                {
                    std::lock_guard<std::mutex> lock(_uuid_session_mtx);
                    _uuid_sessions[new_session->GetUuid()] = new_session;
                }
                new_session->Start();
            }
            else
            {
                std::cerr << "[CServer] Accept error: " << ec.message() << std::endl;
            }
            DoAccept();
        });
}

void CServer::AddUserSession(int uid, std::shared_ptr<CSession> session)
{
    std::lock_guard<std::mutex> lock(_session_mtx);
    _uid_sessions[uid] = session;
    std::cout << "[CServer] User " << uid << " session added." << std::endl;
}

void CServer::RemoveUserSession(int uid)
{
    std::lock_guard<std::mutex> lock(_session_mtx);
    auto it = _uid_sessions.find(uid);
    if (it != _uid_sessions.end())
    {
        _uid_sessions.erase(it);
        std::cout << "[CServer] User " << uid << " session removed." << std::endl;
    }
}

void CServer::ClearSession(const std::string &uuid)
{
    std::lock_guard<std::mutex> lock(_uuid_session_mtx);
    auto it = _uuid_sessions.find(uuid);
    if (it != _uuid_sessions.end())
    {
        _uuid_sessions.erase(it);
        std::cout << "[CServer] Session " << uuid << " cleared." << std::endl;
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
        std::cout << "[CServer] Message forwarded to user " << target_uid << std::endl;
    }
    else
    {
        std::cout << "[CServer] User " << target_uid << " not found, message dropped." << std::endl;
    }
}

#include "CServer.h"
#include "AsioIOServicePool.h"
#include "CSession.h"
#include <iostream>

CServer::CServer(boost::asio::io_context &io_context, short port)
    : _io_context(io_context),
      _port(port),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    std::cout << "Server start success, listen on port : " << _port << std::endl;
}

CServer::~CServer()
{
}

void CServer::Start()
{
    StartAccept();
}

void CServer::StartAccept()
{
    auto &io_context = AsioIOServicePool::GetInstance().GetIOService();
    std::shared_ptr<CSession> new_session = std::make_shared<CSession>(io_context, this);

    _acceptor.async_accept(
        new_session->GetSocket(), [self = shared_from_this(), new_session](const boost::system::error_code &error)
        { self->HandleAccept(new_session, error); });
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code &error)
{
    if (!error)
    {
        new_session->Start();
        std::lock_guard<std::mutex> lock(_mutex);
        _sessions.insert(std::make_pair(new_session->GetUuid(), new_session));
    }
    else
    {
        std::cout << "session accept failed, error is " << error.message() << std::endl;
    }
    StartAccept();
}

void CServer::ClearSession(const std::string &session_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _sessions.erase(session_id);
}
#include "CServer.h"
#include "AsioIOServicePool.h"
#include <functional>
#include <spdlog/spdlog.h>

CServer::CServer(boost::asio::io_context& io_context, short port):_io_context(io_context), _port(port), 
_acceptor(io_context, tcp::endpoint(tcp::v4(),port)) 
{ 
    spdlog::info("Server start success, listen on port : {}", _port); 
    StartAccept(); 
} 

CServer::~CServer() {
    spdlog::info("Server destruct");
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error){ 
    if (!error) { 
        new_session->Start(); 
        lock_guard<mutex> lock(_mutex); 
        _sessions.insert(make_pair(new_session->GetUuid(), new_session)); 
    } 
    else { 
        spdlog::error("session accept failed, error is {}", error.message()); 
    } 

    StartAccept(); 
}

void CServer::StartAccept() { 
    auto io_context = AsioIOServicePool::GetInstance()->GetIOService();
    // AsioIOServicePool::GetIOService returns shared_ptr<io_context>
    // CSession constructor takes io_context&
    shared_ptr<CSession> new_session = make_shared<CSession>(*io_context, this); 
    _acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, std::placeholders::_1)); 
}

void CServer::ClearSession(std::string uuid) {
    lock_guard<mutex> lock(_mutex);
    _sessions.erase(uuid);
}

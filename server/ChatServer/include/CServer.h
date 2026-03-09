#pragma once
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class CSession;

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    CServer(boost::asio::io_context &io_context, short port);
    ~CServer();
    void Start();
    void ClearSession(const std::string &session_id);

private:
    void HandleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code &error);
    void StartAccept();
    boost::asio::io_context &_io_context;
    short _port;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::map<std::string, std::shared_ptr<CSession>> _sessions;
    std::mutex _mutex;
};
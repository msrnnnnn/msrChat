#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <memory>

namespace net = boost::asio;
namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    CServer(net::io_context &ioc, unsigned short &port);
    void Start();

private:
    tcp::acceptor _acceptor;
    net::io_context &_ioc;
    tcp::socket _socket;
};
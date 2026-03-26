#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

static std::string generate_unique_string()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

StatusServiceImpl::StatusServiceImpl()
    : _server_index(0)
{
    auto &cfg = ConfigMgr::GetInstance();
    for (int i = 1; i <= 16; ++i)
    {
        std::string section = "ChatServer" + std::to_string(i);
        ChatServer server;
        server.host = cfg[section]["Host"];
        server.port = cfg[section]["Port"];
        if (!server.host.empty() && !server.port.empty())
        {
            _servers.push_back(server);
        }
    }

    if (_servers.empty())
    {
        ChatServer server;
        server.host = "127.0.0.1";
        server.port = "8080";
        _servers.push_back(server);
    }
}

grpc::Status StatusServiceImpl::GetChatServer(grpc::ServerContext *context, const message::GetChatServerReq *request,
                                              message::GetChatServerRsp *reply)
{
    (void)context;
    (void)request;
    if (_servers.empty())
    {
        reply->set_error(static_cast<int>(ChatApp::ErrorCode::RPCFailed));
        return grpc::Status::OK;
    }

    const auto &server = _servers[_server_index % _servers.size()];
    _server_index = (_server_index + 1) % _servers.size();

    reply->set_host(server.host);
    reply->set_port(server.port);
    reply->set_error(static_cast<int>(ChatApp::ErrorCode::Success));
    reply->set_token(generate_unique_string());
    return grpc::Status::OK;
}

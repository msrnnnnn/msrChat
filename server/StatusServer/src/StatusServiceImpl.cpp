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
    ChatServer server1;
    server1.host = cfg["ChatServer1"]["Host"];
    server1.port = cfg["ChatServer1"]["Port"];
    if (server1.host.empty())
    {
        server1.host = "127.0.0.1";
    }
    if (server1.port.empty())
    {
        server1.port = "8090";
    }
    _servers.push_back(server1);

    ChatServer server2;
    server2.host = cfg["ChatServer2"]["Host"];
    server2.port = cfg["ChatServer2"]["Port"];
    if (server2.host.empty())
    {
        server2.host = "127.0.0.1";
    }
    if (server2.port.empty())
    {
        server2.port = "8091";
    }
    _servers.push_back(server2);
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

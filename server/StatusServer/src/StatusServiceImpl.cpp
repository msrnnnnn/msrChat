#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

std::string generate_unique_string() {
    // 创建UUID对象
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    // 将UUID转换为字符串
    std::string unique_string = to_string(uuid);
    return unique_string;
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
    // 轮询分发，使用 fetch_add 保证线程安全
    int index = _server_index.fetch_add(1) % _servers.size();
    
    auto &server = _servers[index];
    reply->set_host(server.host);
    reply->set_port(server.port);
    reply->set_error((int)ChatApp::ErrorCode::Success);
    reply->set_token(generate_unique_string());
    return Status::OK;
}

StatusServiceImpl::StatusServiceImpl() : _server_index(0)
{
    auto& cfg = ConfigMgr::GetInstance();
    ChatServer server;
    server.port = cfg["ChatServer1"]["Port"];
    server.host = cfg["ChatServer1"]["Host"];
    _servers.push_back(server);
    
    server.port = cfg["ChatServer2"]["Port"];
    server.host = cfg["ChatServer2"]["Host"];
    _servers.push_back(server);
}

#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "const.h"
#include <iostream>

StatusGrpcClient::StatusGrpcClient()
{
    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["StatusServer"]["Host"];
    std::string port = gCfgMgr["StatusServer"]["Port"];
    if (host.empty())
    {
        host = "localhost";
    }
    if (port.empty())
    {
        port = "50052";
    }
    pool_ = std::make_unique<StatusConPool>(5, host, port);
}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
    ClientContext context;
    GetChatServerRsp reply;
    GetChatServerReq request;
    request.set_uid(uid);

    auto stub = pool_->getConnection();
    if (!stub)
    {
        reply.set_error(static_cast<int>(ChatApp::ErrorCode::RPCFailed));
        return reply;
    }
    Status status = stub->GetChatServer(&context, request, &reply);
    pool_->returnConnection(std::move(stub));
    if (!status.ok())
    {
        reply.set_error(static_cast<int>(ChatApp::ErrorCode::RPCFailed));
    }
    return reply;
}

/**
 * @file    StatusGrpcClient.cpp
 * @brief   状态服务 gRPC 客户端实现
 */
#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "const.h"
#include <iostream>

namespace
{
int ParsePositiveInt(const std::string &value, int fallback)
{
    if (value.empty())
    {
        return fallback;
    }
    try
    {
        int parsed = std::stoi(value);
        return parsed > 0 ? parsed : fallback;
    }
    catch (const std::exception &)
    {
        return fallback;
    }
}
} // namespace

StatusGrpcClient::StatusGrpcClient()
{
    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["StatusServer"]["Host"];
    std::string port = gCfgMgr["StatusServer"]["Port"];
    int pool_size = ParsePositiveInt(gCfgMgr["StatusServer"]["PoolSize"], 256);
    
    // 默认配置回退
    if (host.empty())
    {
        host = "localhost";
    }
    if (port.empty())
    {
        port = "50052";
    }
    
    pool_ = std::make_unique<StatusConPool>(static_cast<size_t>(pool_size), host, port);
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

/**
 * @file    StatusGrpcClient.cpp
 * @brief   gRPC 状态服务客户端实现
 * @author  msr
 */

#include "StatusGrpcClient.h"
#include "ConfigMgr.h"
#include "const.h"
#include <iostream>

StatusGrpcClient::StatusGrpcClient()
{
    // ---------------------------------------------------------
    // 1. 读取配置
    // ---------------------------------------------------------
    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["StatusServer"]["Host"];
    std::string port = gCfgMgr["StatusServer"]["Port"];

    // 如果配置读取失败，使用默认值
    if (host.empty())
    {
        host = "localhost";
        std::cout << "[Warning] StatusServer Host not found in config, using default: localhost" << std::endl;
    }
    if (port.empty())
    {
        port = "50052"; // 假设 StatusServer 端口为 50052
        std::cout << "[Warning] StatusServer Port not found in config, using default: 50052" << std::endl;
    }

    std::cout << "StatusGrpcClient config - Host: " << host << ", Port: " << port << std::endl;

    // ---------------------------------------------------------
    // 2. 初始化连接池
    // ---------------------------------------------------------
    pool_ = std::make_unique<StatusConPool>(5, host, port);

    std::cout << "StatusGrpcClient initialized with connection pool." << std::endl;
}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
    ClientContext context;
    GetChatServerRsp reply;
    GetChatServerReq request;
    request.set_uid(uid);

    auto stub = pool_->getConnection();
    Status status = stub->GetChatServer(&context, request, &reply);
    
    // RAII 自动归还连接，无需手动调用 returnConnection
    // pool_->returnConnection(std::move(stub));

    if (status.ok())
    {
        return reply;
    }
    else
    {
        reply.set_error((int)ChatApp::ErrorCode::RPCFailed);
        return reply;
    }
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
    ClientContext context;
    LoginRsp reply;
    LoginReq request;
    request.set_uid(uid);
    request.set_token(token);

    auto stub = pool_->getConnection();
    Status status = stub->Login(&context, request, &reply);
    
    if (status.ok())
    {
        return reply;
    }
    else
    {
        reply.set_error((int)ChatApp::ErrorCode::RPCFailed);
        return reply;
    }
}

/**
 * @file    VerifyGrpcClient.cpp
 * @brief   gRPC 验证服务客户端实现
 */

#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"
#include <iostream>

VerifyGrpcClient::VerifyGrpcClient()
{
    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["VerifyServer"]["Host"];
    std::string port = gCfgMgr["VerifyServer"]["Port"];

    // 默认配置回退
    if (host.empty())
    {
        host = "localhost";
        std::cout << "[Warning] VerifyServer Host not found in config, using default: localhost" << std::endl;
    }
    if (port.empty())
    {
        port = "50051";
        std::cout << "[Warning] VerifyServer Port not found in config, using default: 50051" << std::endl;
    }

    std::cout << "VerifyGrpcClient config - Host: " << host << ", Port: " << port << std::endl;

    // 初始化 gRPC 连接池
    pool_ = std::make_unique<RPConPool>(5, host, port);

    std::cout << "VerifyGrpcClient initialized with connection pool." << std::endl;
}

GetVerifyResponse VerifyGrpcClient::GetVerifyCode(std::string email)
{
    // 模拟 RPC 调用返回
    std::cout << "[Mock] GetVerifyCode called for " << email << std::endl;

    GetVerifyResponse reply;
    reply.set_error(0); 
    reply.set_email(email);
    reply.set_code("123456"); 

    return reply;
}

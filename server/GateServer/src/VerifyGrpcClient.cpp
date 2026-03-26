/**
 * @file    VerifyGrpcClient.cpp
 * @brief   gRPC 验证服务客户端实现
 */

#include "VerifyGrpcClient.h"
#include "ConfigMgr.h"
#include <spdlog/spdlog.h>

VerifyGrpcClient::VerifyGrpcClient()
{
    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["VerifyServer"]["Host"];
    std::string port = gCfgMgr["VerifyServer"]["Port"];

    // 默认配置回退
    if (host.empty())
    {
        host = "localhost";
        spdlog::warn("VerifyServer Host not found in config, using default: localhost");
    }
    if (port.empty())
    {
        port = "50051";
        spdlog::warn("VerifyServer Port not found in config, using default: 50051");
    }

    spdlog::info("VerifyGrpcClient config - Host: {}, Port: {}", host, port);

    // 初始化 gRPC 连接池
    pool_ = std::make_unique<RPConPool>(100, host, port);

    spdlog::info("VerifyGrpcClient initialized with connection pool.");
}

GetVerifyResponse VerifyGrpcClient::GetVerifyCode(std::string email)
{
    // 模拟 RPC 调用返回
    spdlog::info("[Mock] GetVerifyCode called for {}", email);

    GetVerifyResponse reply;
    reply.set_error(0); 
    reply.set_email(email);
    reply.set_code("123456"); 

    return reply;
}

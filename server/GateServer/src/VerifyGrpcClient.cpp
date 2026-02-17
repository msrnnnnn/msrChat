/**
 * @file    VerifyGrpcClient.cpp
 * @brief   gRPC 验证服务客户端实现
 * @author  msr
 */

#include "VerifyGrpcClient.h"
#include "ConfigMgr.h" // 如果你还没有 ConfigMgr，可以先注释掉，看下面的代码
#include <iostream>

VerifyGrpcClient::VerifyGrpcClient()
{
    // ---------------------------------------------------------
    // 1. 读取配置
    // ---------------------------------------------------------
    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string host = gCfgMgr["VerifyServer"]["Host"];
    std::string port = gCfgMgr["VerifyServer"]["Port"];

    // 如果配置读取失败，使用默认值
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

    // ---------------------------------------------------------
    // 2. 初始化连接池
    // 即使现在没有 VerifyServer，这里创建池子也不会崩，
    // 它只会建立 TCP 连接失败，但对象能创建成功。
    // ---------------------------------------------------------
    pool_ = std::make_unique<RPConPool>(5, host, port);

    std::cout << "VerifyGrpcClient initialized with connection pool." << std::endl;
}

GetVerifyResponse VerifyGrpcClient::GetVerifyCode(std::string email)
{
    // =========================================================
    // 方案：既保留了高大上的代码，又能在没有服务器时运行
    // =========================================================

    /* // 【以后用的真代码】
    // 等你有真正的 C++ VerifyServer 跑起来时，把这段解开注释，把下面的 Mock 删掉
    ClientContext context;
    GetVerifyResponse reply;
    GetVerifyRequest request;
    request.set_email(email);

    auto stub = pool_->getConnection();
    Status status = stub->GetVerifyCode(&context, request, &reply);

    if (status.ok()) {
        pool_->returnConnection(std::move(stub));
        return reply;
    } else {
        pool_->returnConnection(std::move(stub));
        reply.set_error(1); // ErrorCodes::RPCFailed
        return reply;
    }
    */

    // 【现在用的 Mock 代码】
    // 假装 RPC 调用成功了
    std::cout << "[Mock] GetVerifyCode called for " << email << std::endl;

    GetVerifyResponse reply;
    reply.set_error(0); // 成功
    reply.set_email(email);
    reply.set_code("123456"); // 永远返回 123456

    return reply;
}

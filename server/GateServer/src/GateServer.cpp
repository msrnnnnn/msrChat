/**
 * @file GateServer.cpp
 * @brief 网关服务器入口
 */

#include "CServer.h"
#include "ConfigMgr.h"
#include "LogicSystem.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "GateServer Starting..." << std::endl;
    std::cout << "========================================" << std::endl;

    auto &gCfgMgr = ConfigMgr::GetInstance();
    std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
    unsigned short gate_port = atoi(gate_port_str.c_str());
    
    if (gate_port == 0)
    {
        gate_port = 8080;
        std::cout << "[Warning] Config load failed or port invalid. Using default port: 8080" << std::endl;
    }
    else
    {
        std::cout << "[Info] GateServer will listen on port: " << gate_port << std::endl;
    }

    // 初始化 LogicSystem（触发路由注册）
    LogicSystem::GetInstance();
    std::cout << "[Info] LogicSystem initialized, routes registered." << std::endl;

    try
    {
        unsigned short port = static_cast<unsigned short>(gate_port);

        // 创建 IO 上下文
        net::io_context ioc{1};

        // 注册信号处理
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code &error, int signal_number) {
            if (error)
            {
                return;
            }
            ioc.stop();
        });

        // 创建服务器实例
        auto server = std::make_shared<CServer>(ioc, port);

        // 启动 Accept 循环
        server->HandleAccept();

        // 运行 IO 事件循环
        ioc.run();
    }
    catch (std::exception const &exp)
    {
        std::cerr << "Error: " << exp.what() << std::endl;
        return EXIT_FAILURE;
    }
}
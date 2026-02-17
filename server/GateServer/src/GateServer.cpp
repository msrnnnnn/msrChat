/**
 * @file GateServer.cpp
 * @brief 网关服务器入口 (Entry Point)
 * @details 负责编排 IO 上下文 (Reactor)、信号处理及服务器对象的生命周期。
 * @author msr
 */

#include "CServer.h"
#include "ConfigMgr.h"
#include "LogicSystem.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

/**
 * @brief 应用程序主入口
 * @return int 程序退出状态码
 * * @note 架构设计说明 (Architectural Notes):
 * 1. **IO 模型**: 采用 Proactor 模式 (Boost.Asio 默认模型)。
 * 2. **事件循环**: 单线程运行 `io_context::run()`，避免了多线程锁竞争，适合 CPU 密集度低的 IO 密集型场景。
 * 3. **信号处理**: 必须捕获 SIGINT/SIGTERM，否则 socket 可能会处于 TIME_WAIT 状态或导致资源泄漏。
 */
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

        /**
         * @brief IO 上下文 (The Reactor Core)
         * @param concurrency_hint {1} 提示内核这是一个单线程驱动的上下文。
         * @details
         * - Linux: 封装了 `epoll_create1` 和 `epoll_wait`。
         * - Windows: 封装了 IOCP (Input/Output Completion Port)。
         */
        net::io_context ioc{1};

        /**
         * @brief 信号集，用于优雅退出
         * @details 注册 SIGINT (Ctrl+C) 和 SIGTERM (kill 命令) 信号。
         * 这是一个异步操作，对应 Linux 的 `signalfd` 机制。
         */
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code &error, int signal_number) {
            if (error)
            {
                return;
            }
            // @warning 必须调用 stop() 中断 run() 的阻塞，否则服务器无法停止。
            ioc.stop();
        });


        /**
         * @brief 创建服务器实例 (RAII)
         * @note 使用 shared_ptr 管理生命周期，引用计数 ref_count = 1。
         */
        auto server = std::make_shared<CServer>(ioc, port);

        // @brief 启动异步 Accept 链条 (The First Domino)
        server->HandleAccept();

        /**
         * @brief 进入事件循环 (Event Loop)
         * @warning **BLOCKING CALL** (阻塞调用)
         * 主线程将在此死循环，处理所有的 Epoll 事件回调。
         * 在此之后的所有业务逻辑（LogicSystem）都会在这个线程串行执行。
         * 严禁在回调中使用 `sleep()` 或执行耗时计算，否则会阻塞整个服务器的网络吞吐。
         */
        ioc.run();
    }
    catch (std::exception const &exp)
    {
        std::cerr << "Error: " << exp.what() << std::endl;
        return EXIT_FAILURE;
    }
}
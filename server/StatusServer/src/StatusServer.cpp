#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include "StatusServiceImpl.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "const.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "AsioIOServicePool.h"

void RunServer() {
    // Initialize spdlog with console and file sinks
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/status_server.log", 1024 * 1024 * 5, 3);
        
        std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("StatusServer", sinks.begin(), sinks.end());
        
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
        return;
    }

    spdlog::info("StatusServer Starting...");

    auto & cfg = ConfigMgr::GetInstance();
    std::string server_address(cfg["StatusServer"]["Host"] + ":" + cfg["StatusServer"]["Port"]);
    
    // Connect to Redis
    auto redisCfg = cfg["Redis"];
    std::string redisHost = redisCfg["Host"];
    std::string redisPort = redisCfg["Port"];
    std::string redisPwd = redisCfg["Passwd"];
    
    if (!RedisMgr::GetInstance()->Connect(redisHost, std::stoi(redisPort))) {
        spdlog::error("Failed to connect to Redis at {}:{}", redisHost, redisPort);
        return;
    }
    if (!redisPwd.empty()) {
        if (!RedisMgr::GetInstance()->Auth(redisPwd)) {
             spdlog::error("Failed to authenticate with Redis");
             return;
        }
    }
    
    StatusServiceImpl service;
    grpc::ServerBuilder builder;
    // 监听端口和添加服务
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    // 构建并启动gRPC服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Server listening on {}", server_address);
    
    // 创建Boost.Asio的io_context
    boost::asio::io_context io_context;
    // 创建signal_set用于捕获SIGINT
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    // 设置异步等待SIGINT信号
    signals.async_wait([&server, &io_context](const boost::system::error_code& error, int signal_number) {
        if (!error) {
            spdlog::info("Shutting down server...");
            server->Shutdown(); // 优雅地关闭服务器
            io_context.stop();
        }
    });
    
    // 在单独的线程中运行io_context
    std::thread([&io_context]() { io_context.run(); }).detach();
    
    // 等待服务器关闭
    server->Wait();
}

int main(int argc, char** argv) {
    try {
        RunServer();
    }
    catch (std::exception const& e) {
        spdlog::error("Error: {}", e.what());
        return EXIT_FAILURE;
    }
    return 0;
}

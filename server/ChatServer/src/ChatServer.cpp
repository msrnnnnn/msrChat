#include "LogicSystem.h" 
#include <csignal> 
#include <thread> 
#include <mutex> 
#include "AsioIOServicePool.h" 
#include "CServer.h" 
#include "ConfigMgr.h" 
#include "MysqlMgr.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>

using namespace std; 

int main() 
{ 
    try {
        // Initialize spdlog with console and file sinks
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("../logs/chat_server.log", 1024 * 1024 * 5, 3);
        
        std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("ChatServer", sinks.begin(), sinks.end());
        
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
        
        spdlog::info("ChatServer Starting...");
        spdlog::info("Current Working Directory: {}", std::filesystem::current_path().string());

        auto &cfg = ConfigMgr::Inst(); 

        // Force Init Mysql Connection Pool
        spdlog::info("Initializing MySQL Connection Pool...");
        MysqlMgr::GetInstance(); 

        auto pool = AsioIOServicePool::GetInstance(); 
        boost::asio::io_context  io_context; 
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM); 
        signals.async_wait([&io_context, pool](auto, auto) { 
            io_context.stop(); 
            pool->Stop(); 
            }); 
        
        // Default to port 8090 if not configured
        std::string port_str = cfg["SelfServer"]["Port"]; 
        if (port_str.empty()) {
            port_str = "8090";
            spdlog::warn("Config Port not found, using default: 8090");
        }

        CServer s(io_context, atoi(port_str.c_str())); 
        spdlog::info("ChatServer listening on port {}", port_str);
        io_context.run(); 
    } 
    catch (std::exception& e) { 
        spdlog::error("Exception: {}", e.what());
    } 
}

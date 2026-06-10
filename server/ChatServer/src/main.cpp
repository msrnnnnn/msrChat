/**
 * @file main.cpp
 * @brief ChatServer 程序入口
 * @details 初始化配置、启动 TCP 监听并注册退出信号。
 */
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "LogicSystem.h"
#include "SQLiteMgr.h"
#include "ImageStorage.h"
#include "TokenManager.h"
#include <boost/asio.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#ifndef _WIN32
#include <unistd.h>
#endif

struct ServerConfig
{
    uint16_t port = 8080;
    std::string db_path = "chatserver.db";
};

/**
 * @brief 加载服务器配置
 * @return ServerConfig 配置结构体
 * @details 从当前目录的 config.ini 读取端口和数据库路径，文件不存在则使用默认值
 */
ServerConfig LoadConfig()
{
    ServerConfig config;
    std::filesystem::path config_path = std::filesystem::current_path() / "config.ini";
    if (std::filesystem::exists(config_path))
    {
        try
        {
            boost::property_tree::ptree pt;
            boost::property_tree::read_ini(config_path.string(), pt);
            int raw_port = pt.get<int>("ChatServer.Port", config.port);
            if (raw_port < 1 || raw_port > 65535)
            {
                spdlog::error("Invalid port in config: {}, must be 1-65535", raw_port);
                return config;
            }
            config.port = static_cast<uint16_t>(raw_port);
            config.db_path = pt.get<std::string>("ChatServer.DbPath", config.db_path);
            spdlog::info("Configuration loaded from {}", config_path.string());
            spdlog::debug("Port: {}, DbPath: {}", config.port, config.db_path);
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Failed to parse config.ini: {}, using defaults", e.what());
        }
    }
    else
    {
        spdlog::warn("config.ini not found, using default configuration");
    }
    return config;
}

/**
 * @brief ChatServer 主入口
 * @param argc 命令行参数个数
 * @param argv 命令行参数列表
 * @return 0 正常退出，1 异常退出
 * @details 初始化流程：解析命令行 → 加载配置 → 初始化 SQLite/ImageStorage/TokenManager
 *           → 创建 CServer 并启动监听 → 注册信号处理 → 进入 I/O 事件循环
 */
int main(int argc, char *argv[])
{
    try
    {
        // 解析 -d/--daemon 守护进程模式参数
        bool daemon_mode = false;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-d" || arg == "--daemon")
            {
                daemon_mode = true;
                break;
            }
        }

        if (daemon_mode)
        {
#ifndef _WIN32
            spdlog::info("Starting in daemon mode...");
            if (daemon(0, 0) != 0)
            {
                std::cerr << "Failed to start daemon" << std::endl;
                return 1;
            }
            spdlog::info("Daemon started successfully with PID: {}", getpid());
#else
            spdlog::warn("Daemon mode is not supported on Windows, running in foreground...");
#endif
        }

        auto config = LoadConfig();

        if (!SQLiteMgr::Instance().Init(config.db_path))
        {
            spdlog::error("Failed to initialize SQLite database at {}", config.db_path);
            return 1;
        }

        if (!ImageStorage::Instance().Init(SQLiteMgr::Instance().GetPool()))
        {
            spdlog::error("Failed to initialize ImageStorage");
            return 1;
        }

        TokenManager::Instance().LoadTokensFromDB();

#ifndef NDEBUG
        const char *dev_token_env = std::getenv("MSRCHAT_DEV_TOKEN");
        if (dev_token_env && dev_token_env[0] != '\0')
        {
            TokenManager::Instance().SetToken(1001, dev_token_env);
        }
        else
        {
            TokenManager::Instance().SetToken(1001, "dev_token");
        }
        spdlog::info("[Main] Dev mode token registered for uid 1001");
#endif

        boost::asio::io_context io_context;
        std::shared_ptr<CServer> server = nullptr;

        auto stop_server = [&server, &io_context](const std::string &signal_name)
        {
            spdlog::info("Received {}, initiating graceful shutdown...", signal_name);

            if (server)
            {
                spdlog::info("Stopping CServer acceptor...");
                server->Stop();
            }

            spdlog::info("Stopping LogicSystem...");
            LogicSystem::getInstance().Shutdown();

            spdlog::info("Stopping AsioIOServicePool...");
            AsioIOServicePool::getInstance().Stop();

            spdlog::info("Shutting down SQLiteMgr...");
            SQLiteMgr::Instance().Shutdown();

            spdlog::info("Graceful shutdown completed");
            io_context.stop();
        };

        auto handle_sigint_sigterm = [stop_server](const boost::system::error_code &ec, int signal_number)
        {
            if (ec)
            {
                spdlog::error("Signal handler error: {}", ec.message());
                return;
            }

            std::string signal_name = (signal_number == SIGINT) ? "SIGINT" : "SIGTERM";
            spdlog::info("Signal {} captured", signal_name);
            stop_server(signal_name);
        };

        // 注册 SIGINT/SIGTERM 信号处理器，实现优雅关闭
        boost::asio::signal_set shutdown_signals(io_context, SIGINT, SIGTERM);
        shutdown_signals.async_wait(handle_sigint_sigterm);

        server = std::make_shared<CServer>(io_context, config.port);
        server->Start();

        spdlog::info("ChatServer is running on port {}...", config.port);
        io_context.run();

        spdlog::info("Main event loop exited");
    }
    catch (const std::exception &e)
    {
        spdlog::error("Exception: {}", e.what());
        return 1;
    }
    return 0;
}

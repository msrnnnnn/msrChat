/**
 * @file main.cpp
 * @brief ChatServer 程序入口
 * @details 初始化配置、启动 TCP 监听并注册退出信号。
 */
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "LogicSystem.h"
#include "SQLiteMgr.h"
#include "TokenManager.h"
#include <boost/asio.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>
#ifndef _WIN32
#include <unistd.h>
#endif

struct ServerConfig
{
    short port = 8080;
    std::string db_path = "chatserver.db";
};

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
            config.port = static_cast<short>(pt.get<int>("ChatServer.Port", config.port));
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

void ReloadConfig()
{
    spdlog::info("Reloading configuration from config.ini...");
    try
    {
        auto config = LoadConfig();
        spdlog::info("Configuration reloaded successfully");
        spdlog::info("New port: {}, New db_path: {}", config.port, config.db_path);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Failed to reload configuration: {}", e.what());
    }
}

int main(int argc, char *argv[])
{
    try
    {
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

        TokenManager::Instance().SetToken(1001, "dev_token");
        spdlog::info("[Main] Dev mode token registered for uid 1001");

        boost::asio::io_context io_context;
        LogicSystem::getInstance().SetIOContext(&io_context);
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
        };

        auto handle_sighup = [](const boost::system::error_code &ec, int signal_number)
        {
            if (ec)
            {
                spdlog::error("SIGHUP handler error: {}", ec.message());
                return;
            }

            spdlog::info("Signal {} captured", signal_number);
            ReloadConfig();
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

        boost::asio::signal_set shutdown_signals(io_context, SIGINT, SIGTERM);
        shutdown_signals.async_wait(handle_sigint_sigterm);

        boost::asio::signal_set reload_signal(io_context, SIGHUP);
        reload_signal.async_wait(handle_sighup);

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

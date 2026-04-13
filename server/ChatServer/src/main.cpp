/**
 * @file main.cpp
 * @brief ChatServer 程序入口
 * @details 初始化配置、启动 TCP 监听并注册退出信号。
 */
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "SQLiteMgr.h"
#include <boost/asio.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <csignal>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <thread>

int main()
{
    try
    {
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);

        signals.async_wait(
            [&io_context](const boost::system::error_code &, int)
            {
                spdlog::info("Stopping server...");
                io_context.stop();
                AsioIOServicePool::getInstance().Stop();
            });

        short port = 8080;
        std::string db_path = "chatserver.db";
        std::filesystem::path config_path = std::filesystem::current_path() / "config.ini";
        if (std::filesystem::exists(config_path))
        {
            boost::property_tree::ptree pt;
            boost::property_tree::read_ini(config_path.string(), pt);
            port = static_cast<short>(pt.get<int>("ChatServer.Port", port));
            db_path = pt.get<std::string>("ChatServer.DbPath", db_path);
        }

        if (!SQLiteMgr::Instance().Init(db_path))
        {
            spdlog::error("Failed to initialize SQLite database at {}", db_path);
            return 1;
        }

        auto server = std::make_shared<CServer>(io_context, port);
        server->Start();

        spdlog::info("ChatServer is running on port {}...", port);
        io_context.run();
    }
    catch (const std::exception &e)
    {
        spdlog::error("Exception: {}", e.what());
    }
    return 0;
}

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
#include <ctime>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/**
 * @brief 生成随机十六进制字符串（仅用于 Debug 构建的 dev token）
 */
static std::string GenerateRandomHexToken(int byte_count)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    std::ostringstream oss;
    for (int i = 0; i < byte_count; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << dist(gen);
    return oss.str();
}

struct ServerConfig
{
    uint16_t port = 8080;
    std::string db_path = "chatserver.db";
    int pool_size = 8; ///< SQLite 连接池大小（1-64）
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
            int raw_pool = pt.get<int>("ChatServer.PoolSize", config.pool_size);
            if (raw_pool < 1 || raw_pool > 64)
            {
                spdlog::warn("Invalid PoolSize in config: {}, must be 1-64, using default {}", raw_pool,
                             config.pool_size);
            }
            else
            {
                config.pool_size = raw_pool;
            }
            spdlog::info("Configuration loaded from {}", config_path.string());
            spdlog::debug("Port: {}, DbPath: {}, PoolSize: {}", config.port, config.db_path, config.pool_size);
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

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_st>("chatserver.log", 50 * 1024 * 1024, 5);
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("chatserver", sinks.begin(), sinks.end());
        spdlog::set_default_logger(logger);

        if (!SQLiteMgr::Instance().Init(config.db_path, config.pool_size))
        {
            spdlog::error("Failed to initialize SQLite database at {}", config.db_path);
            return 1;
        }

        if (!ImageStorage::Instance().Init(SQLiteMgr::Instance().GetPool()))
        {
            spdlog::error("Failed to initialize ImageStorage");
            return 1;
        }

        // Phase 5D.3 — 启动时清理过期图片
        {
            int cleaned = ImageStorage::Instance().DeleteExpired(static_cast<int64_t>(std::time(nullptr)));
            if (cleaned > 0)
            {
                spdlog::info("[Main] Startup: cleaned {} expired images", cleaned);
            }
        }

        TokenManager::Instance().LoadTokensFromDB();

#ifndef NDEBUG
        const char *dev_token_env = std::getenv("MSRCHAT_DEV_TOKEN");
        std::string dev_token;
        if (dev_token_env && dev_token_env[0] != '\0')
        {
            dev_token = dev_token_env;
            spdlog::info("[Main] Dev mode: using MSRCHAT_DEV_TOKEN from environment");
        }
        else
        {
            dev_token = GenerateRandomHexToken(32);
            spdlog::warn("[Main] MSRCHAT_DEV_TOKEN not set, generated random dev token: {}", dev_token);
        }
        TokenManager::Instance().SetToken(1001, dev_token);
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

        // Phase 5D.3 — 定时清理过期图片（每 6 小时）
        auto cleanup_timer = std::make_shared<boost::asio::steady_timer>(io_context);
        std::function<void()> schedule_cleanup;
        schedule_cleanup = [&io_context, cleanup_timer, &schedule_cleanup]()
        {
            cleanup_timer->expires_after(std::chrono::hours(6));
            cleanup_timer->async_wait(
                [&io_context, cleanup_timer, &schedule_cleanup](const boost::system::error_code &ec)
                {
                    if (ec)
                        return; // cancelled or error
                    int cleaned = ImageStorage::Instance().DeleteExpired(static_cast<int64_t>(std::time(nullptr)));
                    if (cleaned > 0)
                    {
                        spdlog::info("[Main] Periodic cleanup: removed {} expired images", cleaned);
                    }
                    schedule_cleanup();
                });
        };
        schedule_cleanup();

        auto backup_timer = std::make_shared<boost::asio::steady_timer>(io_context);
        std::function<void()> schedule_backup;
        schedule_backup = [&io_context, backup_timer, &schedule_backup, &config]()
        {
            backup_timer->expires_after(std::chrono::hours(6));
            backup_timer->async_wait(
                [&io_context, backup_timer, &schedule_backup, &config](const boost::system::error_code &ec)
                {
                    if (ec)
                        return;
                    try
                    {
                        auto now = std::time(nullptr);
                        auto tm = *std::localtime(&now);
                        char time_buf[32];
                        std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H", &tm);
                        std::string backup_name = "chatserver.db.bak." + std::string(time_buf);
                        std::filesystem::copy_file(config.db_path, backup_name,
                                                   std::filesystem::copy_options::overwrite_existing);
                        spdlog::info("[Main] Database backup: {}", backup_name);

                        for (auto &entry : std::filesystem::directory_iterator(std::filesystem::current_path()))
                        {
                            if (entry.path().filename().string().find("chatserver.db.bak.") == 0)
                            {
                                std::string name = entry.path().filename().string();
                                std::string date_part = name.substr(19, 8);
                                if (date_part.size() == 8)
                                {
                                    int year = std::stoi(date_part.substr(0, 4));
                                    int month = std::stoi(date_part.substr(4, 2));
                                    int day = std::stoi(date_part.substr(6, 2));
                                    std::tm entry_tm = {};
                                    entry_tm.tm_year = year - 1900;
                                    entry_tm.tm_mon = month - 1;
                                    entry_tm.tm_mday = day;
                                    std::time_t entry_time = std::mktime(&entry_tm);
                                    double diff_days = std::difftime(now, entry_time) / 86400.0;
                                    if (diff_days > 7)
                                    {
                                        std::filesystem::remove(entry.path());
                                        spdlog::info("[Main] Removed old backup: {}", name);
                                    }
                                }
                            }
                        }
                    }
                    catch (const std::exception &e)
                    {
                        spdlog::error("[Main] Backup failed: {}", e.what());
                    }
                    schedule_backup();
                });
        };
        schedule_backup();

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

#include "AsioIOServicePool.h"
#include "CServer.h"
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
        auto &pool = AsioIOServicePool::getInstance();
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
        std::string gate_host = "127.0.0.1";
        std::string gate_port = "8080";
        std::filesystem::path config_path = std::filesystem::current_path() / "config.ini";
        if (std::filesystem::exists(config_path))
        {
            boost::property_tree::ptree pt;
            boost::property_tree::read_ini(config_path.string(), pt);
            port = static_cast<short>(pt.get<int>("ChatServer.Port", port));
            gate_host = pt.get<std::string>("GateServer.Host", gate_host);
            gate_port = pt.get<std::string>("GateServer.Port", gate_port);
        }

        auto server = std::make_shared<CServer>(io_context, port);
        server->SetAuthServer(gate_host, gate_port);
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

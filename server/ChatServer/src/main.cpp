#include "AsioIOServicePool.h"
#include "CServer.h"
#include <boost/asio.hpp>
#include <csignal>
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

        short port = 8080; // 写死端口快速测试
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

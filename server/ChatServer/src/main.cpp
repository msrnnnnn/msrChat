#include "AsioIOServicePool.h"
#include "CServer.h"
#include <boost/asio.hpp>
#include <csignal>
#include <iostream>
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
                std::cout << "Stopping server..." << std::endl;
                io_context.stop();
                AsioIOServicePool::getInstance().Stop();
            });

        short port = 8080; // 写死端口快速测试
        auto server = std::make_shared<CServer>(io_context, port);
        server->Start();

        std::cout << "ChatServer is running on port " << port << "..." << std::endl;
        io_context.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
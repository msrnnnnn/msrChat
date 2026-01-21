#include "CServer.h"
#include <iostream>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
int main()
{
    try
    {
        unsigned short port = static_cast<unsigned short>(8080);
        net::io_context ioc{1};
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
            [&ioc](const boost::system::error_code &error, int signal_number)
            {
                if (error)
                {
                    return;
                }
                ioc.stop();
            });
        auto server = std::make_shared<CServer>(ioc, port);
        // 启动监听
        server->HandleAccept();
        ioc.run();
    }
    catch (std::exception const &exp)
    {
        std::cerr << "Error: " << exp.what() << std::endl;
        return EXIT_FAILURE;
    }
}
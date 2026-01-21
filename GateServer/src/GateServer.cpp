#include "CServer.h"
#include <iostream>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
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
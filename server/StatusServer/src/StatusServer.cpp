#include "ConfigMgr.h"
#include "StatusServiceImpl.h"

#include <boost/asio.hpp>
#include <csignal>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <spdlog/spdlog.h>

static void RunServer()
{
    auto &cfg = ConfigMgr::GetInstance();
    std::string host = cfg["StatusServer"]["Host"];
    std::string port = cfg["StatusServer"]["Port"];
    if (host.empty())
    {
        host = "0.0.0.0";
    }
    if (port.empty())
    {
        port = "50052";
    }
    std::string server_address = host + ":" + port;

    StatusServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Server listening on {}", server_address);

    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&server](const boost::system::error_code &error, int)
                       {
                           if (!error)
                           {
                               spdlog::info("Shutting down server...");
                               server->Shutdown();
                           }
                       });

    std::thread([&io_context]()
                { io_context.run(); })
        .detach();

    server->Wait();
    io_context.stop();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    try
    {
        RunServer();
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error: {}", e.what());
        return EXIT_FAILURE;
    }
    return 0;
}

#include "LogicSystem.h" 
#include <csignal> 
#include <thread> 
#include <mutex> 
#include "AsioIOServicePool.h" 
#include "CServer.h" 
#include "ConfigMgr.h" 
#include <iostream>

using namespace std; 

int main() 
{ 
    try { 
        auto &cfg = ConfigMgr::Inst(); 
        auto pool = AsioIOServicePool::GetInstance(); 
        boost::asio::io_context  io_context; 
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM); 
        signals.async_wait([&io_context, pool](auto, auto) { 
            io_context.stop(); 
            pool->Stop(); 
            }); 
        
        // Default to port 8090 if not configured
        std::string port_str = cfg["SelfServer"]["Port"]; 
        if (port_str.empty()) {
            port_str = "8090";
            cout << "Config Port not found, using default: 8090" << endl;
        }

        CServer s(io_context, atoi(port_str.c_str())); 
        io_context.run(); 
    } 
    catch (std::exception& e) { 
        std::cerr << "Exception: " << e.what() << endl; 
    } 
}

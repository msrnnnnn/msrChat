#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include <vector>
#include <atomic>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

struct ChatServer {
    std::string host;
    std::string port;
};

class StatusServiceImpl final : public StatusService::Service
{
public:
    StatusServiceImpl();
    Status GetChatServer(ServerContext* context, const GetChatServerReq* request,
        GetChatServerRsp* reply) override;
    
private:
    std::vector<ChatServer> _servers;
    std::atomic<int> _server_index;
};

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include <vector>
#include <atomic>

#include <mutex>
#include <map>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
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
    Status Login(ServerContext* context, const LoginReq* request, LoginRsp* reply) override;
    void insertToken(int uid, std::string token);
    
private:
    std::vector<ChatServer> _servers;
    std::atomic<int> _server_index;
    std::map<int, std::string> _tokens;
    std::mutex _token_mtx;
};

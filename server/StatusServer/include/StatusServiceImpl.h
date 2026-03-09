#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <vector>

#include "message.grpc.pb.h"

struct ChatServer
{
    std::string host;
    std::string port;
};

class StatusServiceImpl final : public message::StatusService::Service
{
public:
    StatusServiceImpl();
    grpc::Status GetChatServer(grpc::ServerContext *context, const message::GetChatServerReq *request,
                               message::GetChatServerRsp *reply) override;

private:
    std::vector<ChatServer> _servers;
    std::size_t _server_index;
};

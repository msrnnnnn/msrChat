#pragma once
#include "Singleton.h"
#include "message.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using message::GetVerifyRequest;
using message::GetVerifyResponse;
using message::VerifyService;

class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;

public:
    GetVerifyResponse GetVerifyCode(std::string email);

private:
    VerifyGrpcClient();
};
#include "VerifyGrpcClient.h"
#include <iostream>

VerifyGrpcClient::VerifyGrpcClient()
{
    // [Mock] 不创建连接
    std::cout << "VerifyGrpcClient (Mock) Initialized." << std::endl;
}

GetVerifyResponse VerifyGrpcClient::GetVerifyCode(std::string email)
{
    std::cout << "[Mock] Getting verify code for: " << email << std::endl;

    // [Mock] 直接构造成功响应，假装服务器返回了
    GetVerifyResponse reply;
    reply.set_error(0); // ErrorCodes::Success
    reply.set_email(email);
    reply.set_code("123456"); // 永远返回 123456

    return reply;
}
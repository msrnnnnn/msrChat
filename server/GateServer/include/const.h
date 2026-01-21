#pragma once
namespace ChatApp
{
enum class ErrorCode
{
    Success = 0,
    Error_Json = 1001, // Json解析错误
    RPCFailed = 1002,  // RPC请求错误
};
} 
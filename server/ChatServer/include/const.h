#pragma once
#include <functional>

namespace ChatApp
{
    enum class ErrorCode
    {
        Success = 0,
        Error_Json = 1001,
        RPCFailed = 1002,
        VarifyExpired = 1003,
        VarifyCodeErr = 1004,
        UserExist = 1005,
        PasswdErr = 1006,
        EmailNotMatch = 1007,
        PasswdUpFailed = 1008,
        PasswdInvalid = 1009,
        RPCGetFailed = 1010,
        UidInvalid = 1011,
        TokenInvalid = 1012
    };

    class Defer {
    public:
        Defer(std::function<void()> func) : func_(func) {}
        ~Defer() { func_(); }
    private:
        std::function<void()> func_;
    };
}

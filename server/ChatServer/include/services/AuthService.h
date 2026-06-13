#pragma once
/**
 * @file AuthService.h
 * @brief 认证相关消息处理器
 * @details 登录、注册、登录认证、验证码、重置密码
 */

#include "CSession.h"
#include <string>

class AuthService
{
public:
    static bool HandleLoginRequest(CSession &session, const std::string &body_data);
    static bool HandleRegisterRequest(CSession &session, const std::string &body_data);
    static bool HandleLoginAuthRequest(CSession &session, const std::string &body_data);
    static bool HandleGetVerifyCodeRequest(CSession &session, const std::string &body_data);
    static bool HandleResetPwdRequest(CSession &session, const std::string &body_data);
};

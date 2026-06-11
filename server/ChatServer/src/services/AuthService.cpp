/**
 * @file    AuthService.cpp
 * @brief   认证相关消息处理器实现
 * @details 登录、注册、登录认证、验证码、重置密码
 */

#include "services/AuthService.h"
#include "services/DispatchGuard.h"
#include "CServer.h"
#include "NonceCache.h"
#include "SQLiteMgr.h"
#include "AuthRepository.h"
#include "TokenManager.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <cctype>

// ─── HandleLoginRequest (MSG_CHAT_LOGIN 1005) ───

bool AuthService::HandleLoginRequest(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    auto json_data = nlohmann::json::parse(body_data, nullptr, false);
    nlohmann::json response;

    if (json_data.is_discarded())
    {
        response["error"] = ERR_PARSE_ERROR;
        response["message"] = "登录数据无效";
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        return true;
    }

    int uid = json_data.value("uid", 0);
    std::string token = json_data.value("token", "");

    if (uid <= 0 || token.empty())
    {
        response["error"] = ERR_NETWORK;
        response["message"] = "登录参数无效";
        response["uid"] = uid;
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        return true;
    }

    if (session.GetUserUid() != 0)
    {
        response["error"] = ERR_NETWORK;
        response["message"] = "已登录";
        response["uid"] = session.GetUserUid();
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        return true;
    }

    // CAS 原子操作：防止同一连接并发重复登录
    bool expected = false;
    if (!session.TrySetLoginInProgress(expected))
    {
        response["error"] = ERR_NETWORK;
        response["message"] = "登录进行中";
        response["uid"] = uid;
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        return true;
    }

    auto server = session.GetServer();
    bool token_valid = false;
    if (server)
    {
        token_valid = TokenManager::Instance().CheckToken(uid, token);
    }
    session.OnLoginValidated(uid, token_valid);
    return true;
}

// ─── HandleRegisterRequest (ID_REGISTER_USER 1002) ───

bool AuthService::HandleRegisterRequest(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string password_hash = json_data.value("passwd", "");
        std::string email = json_data.value("email", "");
        std::string verifycode = json_data.value("verifycode", "");

        // Phase 6.4: 防重放校验（兼容旧客户端：nonce 为空时跳过）
        std::string nonce = json_data.value("nonce", "");
        if (!nonce.empty())
        {
            int64_t timestamp = json_data.value("timestamp", 0LL);
            if (!NonceCache::Instance().IsWithinTimeWindow(timestamp))
            {
                nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "请求已过期"}};
                session.Send(response.dump(), ID_REGISTER_USER);
                return true;
            }
            if (!NonceCache::Instance().TryInsert(nonce))
            {
                nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "重复请求"}};
                session.Send(response.dump(), ID_REGISTER_USER);
                return true;
            }
        }

        if (username.empty() || password_hash.empty() || email.empty() || verifycode.empty())
        {
            nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "请填写所有必填项"}};
            session.Send(response.dump(), ID_REGISTER_USER);
            return true;
        }

        if (username.size() < 3 || username.size() > 20)
        {
            nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "用户名长度应为3-20个字符"}};
            session.Send(response.dump(), ID_REGISTER_USER);
            return true;
        }
        for (char c : username)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "用户名只能包含字母、数字和下划线"}};
                session.Send(response.dump(), ID_REGISTER_USER);
                return true;
            }
        }

        if (email.size() < 5 || email.size() > 254 || email.find('@') == std::string::npos
            || email.find('.') == std::string::npos)
        {
            nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "邮箱格式不正确"}};
            session.Send(response.dump(), ID_REGISTER_USER);
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, username, password_hash, email, verifycode]()
            {
                if (safe_session->IsClosed()) return;

                int verifyResult = SQLiteMgr::Instance().Auth().CheckVerifyCode(email, verifycode);
                if (verifyResult != 0)
                {
                    nlohmann::json response;
                    response["error"] = verifyResult;
                    safe_session->Send(response.dump(), ID_REGISTER_USER);
                    safe_session->ContinueReading();
                    return;
                }

                AuthResult result = SQLiteMgr::Instance().Auth().RegisterUser(username, password_hash, email);

                nlohmann::json response;
                response["error"] = result.error;
                if (result.error == 0)
                {
                    response["uid"] = result.uid;
                    response["user"] = result.username;
                    response["token"] = result.token;
                    response["email"] = email;
                }
                safe_session->Send(response.dump(), ID_REGISTER_USER);
                safe_session->ContinueReading();
            });

        // 异步 lambda 自行负责 ContinueReading
        guard.Release();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[AuthService] HandleRegisterRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_PARSE_ERROR}};
        session.Send(response.dump(), ID_REGISTER_USER);
    }
    catch (...)
    {
        spdlog::error("[AuthService] HandleRegisterRequest unknown exception");
        nlohmann::json response{{"error", ERR_PARSE_ERROR}};
        session.Send(response.dump(), ID_REGISTER_USER);
    }
    return true;
}

// ─── HandleLoginAuthRequest (ID_LOGIN_USER 1004) ───

bool AuthService::HandleLoginAuthRequest(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string password_hash = json_data.value("passwd", "");

        if (username.empty() || password_hash.empty())
        {
            nlohmann::json response;
            response["error"] = ERR_INVALID_PARAM;
            response["message"] = "参数无效";
            session.Send(response.dump(), ID_LOGIN_USER);
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, server, username, password_hash]()
            {
                if (safe_session->IsClosed()) return;

                AuthResult result = SQLiteMgr::Instance().Auth().LoginUser(username, password_hash);

                nlohmann::json response;
                response["error"] = result.error;
                if (result.error != 0)
                {
                    safe_session->Send(response.dump(), ID_LOGIN_USER);
                    safe_session->ContinueReading();
                    return;
                }

                TokenManager::Instance().SetToken(result.uid, result.token);
                response["uid"] = result.uid;
                response["user"] = result.username;
                response["token"] = result.token;
                spdlog::info("[AuthService] User {} auth login success, token issued", result.uid);

                safe_session->Send(response.dump(), ID_LOGIN_USER);
                safe_session->ContinueReading();
            });

        guard.Release();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[AuthService] HandleLoginAuthRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_PARSE_ERROR}};
        session.Send(response.dump(), ID_LOGIN_USER);
    }
    catch (...)
    {
        spdlog::error("[AuthService] HandleLoginAuthRequest unknown exception");
        nlohmann::json response{{"error", ERR_PARSE_ERROR}};
        session.Send(response.dump(), ID_LOGIN_USER);
    }
    return true;
}

// ─── HandleGetVerifyCodeRequest (ID_GET_VERIFY_CODE 1001) ───

bool AuthService::HandleGetVerifyCodeRequest(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string email = json_data.value("email", "");

        if (email.empty())
        {
            nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "邮箱不能为空"}};
            session.Send(response.dump(), ID_GET_VERIFY_CODE);
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, email]()
            {
                if (safe_session->IsClosed()) return;

                int code = 0;
                bool success = SQLiteMgr::Instance().Auth().SendVerifyCode(email, code);

                nlohmann::json response{{"error", success ? ERR_SUCCESS : ERR_INVALID_PARAM}, {"email", email}, {"code", code}};
                if (!success)
                {
                    response["message"] = "验证码发送失败";
                }
                safe_session->Send(response.dump(), ID_GET_VERIFY_CODE);
                safe_session->ContinueReading();
            });

        guard.Release();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[AuthService] HandleGetVerifyCodeRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_PARSE_ERROR}, {"message", "请求处理异常"}};
        session.Send(response.dump(), ID_GET_VERIFY_CODE);
    }
    catch (...)
    {
        spdlog::error("[AuthService] HandleGetVerifyCodeRequest unknown exception");
        nlohmann::json response{{"error", ERR_PARSE_ERROR}, {"message", "未知错误"}};
        session.Send(response.dump(), ID_GET_VERIFY_CODE);
    }
    return true;
}

// ─── HandleResetPwdRequest (ID_RESET_PWD 1003) ───

bool AuthService::HandleResetPwdRequest(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string email = json_data.value("email", "");
        std::string code = json_data.value("verifycode", "");
        std::string new_password_hash = json_data.value("passwd", "");

        // Phase 6.4: 防重放校验（兼容旧客户端：nonce 为空时跳过）
        std::string nonce = json_data.value("nonce", "");
        if (!nonce.empty())
        {
            int64_t timestamp = json_data.value("timestamp", 0LL);
            if (!NonceCache::Instance().IsWithinTimeWindow(timestamp))
            {
                nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "请求已过期"}};
                session.Send(response.dump(), ID_RESET_PWD);
                return true;
            }
            if (!NonceCache::Instance().TryInsert(nonce))
            {
                nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "重复请求"}};
                session.Send(response.dump(), ID_RESET_PWD);
                return true;
            }
        }

        if (username.empty() || email.empty() || code.empty() || new_password_hash.empty())
        {
            nlohmann::json response{{"error", ERR_INVALID_PARAM}, {"message", "请填写所有必填项"}};
            session.Send(response.dump(), ID_RESET_PWD);
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, username, email, code, new_password_hash]()
            {
                if (safe_session->IsClosed()) return;

                int verify_result = SQLiteMgr::Instance().Auth().CheckVerifyCode(email, code);
                int error_code = verify_result;
                std::string new_token;

                if (verify_result == 0)
                {
                    int reset_result = SQLiteMgr::Instance().Auth().ResetPassword(username, email, code, new_password_hash);
                    if (reset_result == 0)
                    {
                        error_code = 0;
                        auto user = SQLiteMgr::Instance().Auth().GetUserByUsername(username);
                        if (user.has_value())
                        {
                            int uid = user->uid;
                            AuthResult login_result = SQLiteMgr::Instance().Auth().LoginUser(username, new_password_hash);
                            if (login_result.error == 0)
                            {
                                new_token = login_result.token;
                                auto srv = safe_session->GetServer();
                                if (srv) TokenManager::Instance().SetToken(uid, new_token);
                            }
                        }
                    }
                    else
                    {
                        error_code = reset_result;
                    }
                }

                nlohmann::json response{{"error", error_code}};
                if (error_code == 0 && !new_token.empty())
                {
                    response["token"] = new_token;
                    response["message"] = "密码已重置";
                }
                else
                {
                    switch (error_code)
                    {
                        case ERR_VERIFY_EXPIRED: response["message"] = "验证码已过期"; break;
                        case ERR_VERIFY_WRONG: response["message"] = "验证码错误"; break;
                        case ERR_USER_NOT_EXIST: response["message"] = "用户不存在"; break;
                        case ERR_EMAIL_NOT_MATCH: response["message"] = "邮箱不匹配"; break;
                        case ERR_PASSWD_UPDATE: response["message"] = "密码更新失败"; break;
                        default: response["message"] = "重置密码失败"; break;
                    }
                }
                safe_session->Send(response.dump(), ID_RESET_PWD);
                safe_session->ContinueReading();
            });

        guard.Release();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[AuthService] HandleResetPwdRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_PARSE_ERROR}, {"message", "请求处理异常"}};
        session.Send(response.dump(), ID_RESET_PWD);
    }
    catch (...)
    {
        spdlog::error("[AuthService] HandleResetPwdRequest unknown exception");
        nlohmann::json response{{"error", ERR_PARSE_ERROR}, {"message", "未知错误"}};
        session.Send(response.dump(), ID_RESET_PWD);
    }
    return true;
}

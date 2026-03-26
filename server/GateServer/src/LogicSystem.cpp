/**
 * @file    LogicSystem.cpp
 * @brief   业务逻辑分发系统实现
 */

#include "LogicSystem.h"
#include "HttpConnection.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "StatusGrpcClient.h"
#include "VerifyGrpcClient.h"
#include "const.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <algorithm>
#include <cctype>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace
{
std::size_t GetBusinessThreadCount()
{
    return 200;
}

bool IsUuidToken(const std::string &token)
{
    if (token.size() != 36)
    {
        return false;
    }

    for (size_t i = 0; i < token.size(); ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (token[i] != '-')
            {
                return false;
            }
            continue;
        }

        const unsigned char ch = static_cast<unsigned char>(token[i]);
        if (!std::isxdigit(ch))
        {
            return false;
        }
    }

    return true;
}
} // namespace

LogicSystem::LogicSystem()
    : _business_pool(GetBusinessThreadCount())
{
    RegisterGet(
        "/get_test",
        [](std::shared_ptr<HttpConnection> connection)
        {
            connection->_response.result(http::status::ok);
            connection->_response.set(http::field::server, "GateServer");
            connection->_response.set(http::field::content_type, "text/plain");
            beast::ostream(connection->_response.body()) << "receive get_test req " << std::endl;

            int index = 0;
            for (const auto &elem : connection->_get_params)
            {
                ++index;
                beast::ostream(connection->_response.body()) << "param" << index << " key is " << elem.first;
                beast::ostream(connection->_response.body()) << ", value is " << elem.second << std::endl;
            }

            connection->WriteResponse();
        });

    RegisterPost(
        "/get_varifycode",
        [this](std::shared_ptr<HttpConnection> connection)
        {
            const auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::info("receive body is {}", body_str);

            Json::Value request_json;
            Json::Reader reader;
            if (!reader.parse(body_str, request_json))
            {
                Json::Value response_json;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                WriteJsonResponse(connection, response_json.toStyledString());
                return;
            }

            const std::string email = request_json["email"].asString();
            DispatchBusinessTask(
                connection,
                [email]()
                {
                    Json::Value response_json;
                    GetVerifyResponse rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
                    const std::string code = rsp.code();
                    RedisMgr::GetInstance()->Set(email, code);
                    response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
                    response_json["code"] = code;
                    response_json["email"] = email;
                    return response_json.toStyledString();
                });
        });

    RegisterPost(
        "/user_register",
        [this](std::shared_ptr<HttpConnection> connection)
        {
            const auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::info("receive body is {}", body_str);

            Json::Value request_json;
            Json::Reader reader;
            if (!reader.parse(body_str, request_json))
            {
                Json::Value response_json;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                WriteJsonResponse(connection, response_json.toStyledString());
                return;
            }

            const std::string user = request_json["user"].asString();
            const std::string email = request_json["email"].asString();
            const std::string passwd = request_json["passwd"].asString();
            const std::string verify_code = request_json["varifycode"].asString();

            DispatchBusinessTask(
                connection,
                [user, email, passwd, verify_code]()
                {
                    Json::Value response_json;
                    std::string stored_verify_code;
                    if (!RedisMgr::GetInstance()->Get(email, stored_verify_code))
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyExpired);
                        return response_json.toStyledString();
                    }

                    if (stored_verify_code != verify_code)
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyCodeErr);
                        return response_json.toStyledString();
                    }

                    if (RedisMgr::GetInstance()->ExistsKey(user))
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserExist);
                        return response_json.toStyledString();
                    }

                    const int uid = MysqlMgr::GetInstance()->RegUser(user, email, passwd, "");
                    if (uid == -2)
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::ServerBusy);
                        return response_json.toStyledString();
                    }

                    if (uid == 0 || uid == -1)
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserExist);
                        return response_json.toStyledString();
                    }

                    response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
                    response_json["uid"] = uid;
                    response_json["email"] = email;
                    response_json["user"] = user;
                    return response_json.toStyledString();
                });
        });

    RegisterPost(
        "/reset_pwd",
        [this](std::shared_ptr<HttpConnection> connection)
        {
            const auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

            Json::Value request_json;
            Json::Reader reader;
            if (!reader.parse(body_str, request_json))
            {
                Json::Value response_json;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                WriteJsonResponse(connection, response_json.toStyledString());
                return;
            }

            const std::string user = request_json["user"].asString();
            const std::string email = request_json["email"].asString();
            const std::string passwd = request_json["passwd"].asString();
            const std::string verify_code = request_json["varifycode"].asString();

            DispatchBusinessTask(
                connection,
                [user, email, passwd, verify_code]()
                {
                    Json::Value response_json;
                    std::string stored_verify_code;
                    if (!RedisMgr::GetInstance()->Get(email, stored_verify_code))
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyExpired);
                        return response_json.toStyledString();
                    }

                    if (stored_verify_code != verify_code)
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyCodeErr);
                        return response_json.toStyledString();
                    }

                    const int uid = MysqlMgr::GetInstance()->ResetPwd(user, email, passwd);
                    if (uid == 0)
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::EmailNotMatch);
                        return response_json.toStyledString();
                    }

                    if (uid == -1)
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::PasswdUpFailed);
                        return response_json.toStyledString();
                    }

                    response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
                    response_json["uid"] = uid;
                    response_json["email"] = email;
                    response_json["user"] = user;
                    return response_json.toStyledString();
                });
        });

    RegisterPost(
        "/user_login",
        [this](std::shared_ptr<HttpConnection> connection)
        {
            const auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

            Json::Value request_json;
            Json::Reader reader;
            if (!reader.parse(body_str, request_json))
            {
                Json::Value response_json;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                WriteJsonResponse(connection, response_json.toStyledString());
                return;
            }

            const std::string user = request_json["user"].asString();
            const std::string passwd = request_json["passwd"].asString();

            DispatchBusinessTask(
                connection,
                [user, passwd]()
                {
                    Json::Value response_json;
                    UserInfo user_info;
                    if (!MysqlMgr::GetInstance()->CheckPwd(user, passwd, user_info))
                    {
                        if (user_info.uid == 0)
                        {
                            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserNotExist);
                        }
                        else if (user_info.uid == -1)
                        {
                            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::PasswdErr);
                        }
                        else
                        {
                            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::ServerBusy);
                        }
                        return response_json.toStyledString();
                    }

                    GetChatServerRsp reply = StatusGrpcClient::GetInstance()->GetChatServer(user_info.uid);
                    if (reply.error() != static_cast<int>(ChatApp::ErrorCode::Success))
                    {
                        response_json["error"] = static_cast<int>(ChatApp::ErrorCode::RPCGetFailed);
                        return response_json.toStyledString();
                    }

                    RedisMgr::GetInstance()->Set("token:" + std::to_string(user_info.uid), reply.token());
                    response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
                    response_json["user"] = user;
                    response_json["uid"] = user_info.uid;
                    response_json["token"] = reply.token();
                    response_json["host"] = reply.host();
                    response_json["port"] = reply.port();
                    return response_json.toStyledString();
                });
        });

    RegisterPost(
        "/verify_token",
        [this](std::shared_ptr<HttpConnection> connection)
        {
            const auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());

            Json::Value request_json;
            Json::Value response_json;
            Json::Reader reader;
            if (!reader.parse(body_str, request_json))
            {
                response_json["error"] = 1;
                response_json["message"] = "invalid json";
                WriteJsonResponse(connection, response_json.toStyledString());
                return;
            }

            const int uid = request_json.get("uid", 0).asInt();
            const std::string token = request_json.get("token", "").asString();
            if (uid <= 0 || token.empty())
            {
                response_json["error"] = 1;
                response_json["message"] = "invalid params";
                WriteJsonResponse(connection, response_json.toStyledString());
                return;
            }

            DispatchBusinessTask(
                connection,
                [uid, token]()
                {
                    Json::Value response_json;
                    if (token == "dev_token")
                    {
                        response_json["error"] = 0;
                        response_json["message"] = "login success";
                        response_json["uid"] = uid;
                        return response_json.toStyledString();
                    }

                    std::string stored_token;
                    const bool ok = RedisMgr::GetInstance()->Get("token:" + std::to_string(uid), stored_token);
                    if (!ok || stored_token != token)
                    {
                        if (IsUuidToken(token) && MysqlMgr::GetInstance()->UserExistsByUid(uid))
                        {
                            RedisMgr::GetInstance()->Set("token:" + std::to_string(uid), token);
                            response_json["error"] = 0;
                            response_json["message"] = "login success";
                            response_json["uid"] = uid;
                            return response_json.toStyledString();
                        }

                        response_json["error"] = 1;
                        response_json["message"] = "token invalid";
                        response_json["uid"] = uid;
                        return response_json.toStyledString();
                    }

                    response_json["error"] = 0;
                    response_json["message"] = "login success";
                    response_json["uid"] = uid;
                    return response_json.toStyledString();
                });
        });
}

LogicSystem::~LogicSystem()
{
    _business_pool.join();
}

void LogicSystem::DispatchBusinessTask(std::shared_ptr<HttpConnection> connection, std::function<std::string()> task)
{
    boost::asio::post(
        _business_pool,
        [this, connection, task = std::move(task)]() mutable
        {
            std::string body;
            try
            {
                body = task();
            }
            catch (const std::exception &exp)
            {
                spdlog::error("business task failed: {}", exp.what());
                Json::Value response_json;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::ServerBusy);
                body = response_json.toStyledString();
            }

            WriteJsonResponse(connection, std::move(body));
        });
}

void LogicSystem::WriteJsonResponse(std::shared_ptr<HttpConnection> connection, std::string body)
{
    boost::asio::dispatch(
        connection->_socket.get_executor(),
        [connection, body = std::move(body)]() mutable
        {
            if (!connection->_socket.is_open())
            {
                return;
            }

            connection->_response = {};
            connection->_response.version(connection->_request.version());
            connection->_response.keep_alive(connection->_request.keep_alive());
            connection->_response.result(http::status::ok);
            connection->_response.set(http::field::server, "GateServer");
            connection->_response.set(http::field::content_type, "text/json");
            beast::ostream(connection->_response.body()) << body;
            connection->WriteResponse();
        });
}

void LogicSystem::RegisterGet(std::string url, HttpHandler handler)
{
    _registerGet.emplace(std::move(url), std::move(handler));
}

void LogicSystem::RegisterPost(std::string url, HttpHandler handler)
{
    _registerPost.emplace(std::move(url), std::move(handler));
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> connection)
{
    auto it = _registerGet.find(path);
    if (it == _registerGet.end())
    {
        return false;
    }

    it->second(std::move(connection));
    return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> connection)
{
    auto it = _registerPost.find(path);
    if (it == _registerPost.end())
    {
        return false;
    }

    it->second(std::move(connection));
    return true;
}

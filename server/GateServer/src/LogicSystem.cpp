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
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

LogicSystem::LogicSystem()
{
    // 注册测试路由
    RegisterGet(
        "/get_test",
        [](std::shared_ptr<HttpConnection> connection)
        {
            beast::ostream(connection->_response.body()) << "receive get_test req " << std::endl;
            int i = 0;
            for (auto &elem : connection->_get_params)
            {
                i++;
                beast::ostream(connection->_response.body()) << "param" << i << " key is " << elem.first;
                beast::ostream(connection->_response.body()) << ", " << " value is " << elem.second << std::endl;
            }
        });

    // 注册获取验证码路由
    RegisterPost(
        "/get_varifycode",
        [](std::shared_ptr<HttpConnection> connection)
        {
            // 读取请求体并将 buffer 转换为 string
            auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            std::cout << "receive body is " << body_str << std::endl;

            connection->_response.set(http::field::content_type, "text/json");
            Json::Value response_json;
            Json::Value request_json;
            Json::Reader reader;

            // 解析 JSON 数据
            bool parse_success = reader.parse(body_str, request_json);
            if (!parse_success)
            {
                std::cout << "Failed to parse JSON data!" << std::endl;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                std::string jsonstr = response_json.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }

            // 提取 email 字段
            auto email = request_json["email"].asString();
            std::cout << "email is " << email << std::endl;

            // 调用 gRPC 客户端获取验证码
            GetVerifyResponse rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
            std::string code = rsp.code();
            std::cout << "get varify code is " << code << std::endl;
            
            response_json["code"] = code;
            response_json["email"] = email;
            
            // 将验证码写入 Redis
            RedisMgr::GetInstance()->Set(email, code);

            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
            std::string jsonstr = response_json.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        });

    RegisterPost(
        "/user_register",
        [](std::shared_ptr<HttpConnection> connection)
        {
            auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            std::cout << "receive body is " << body_str << std::endl;
            
            connection->_response.set(http::field::content_type, "text/json");
            Json::Value response_json;
            Json::Value request_json;
            Json::Reader reader;
            
            bool parse_success = reader.parse(body_str, request_json);
            if (!parse_success)
            {
                std::cout << "Failed to parse JSON data!" << std::endl;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                std::string jsonstr = response_json.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }
            // 验证码校验
            std::string varify_code;
            bool b_get_varify = RedisMgr::GetInstance()->Get(request_json["email"].asString(), varify_code);
            if (!b_get_varify)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyExpired);
                std::string jsonstr = response_json.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }
            if (varify_code != request_json["varifycode"].asString())
            {
                std::cout << " varify code error" << std::endl;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyCodeErr);
                std::string jsonstr = response_json.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }
            // 访问redis查找
            bool b_usr_exist = RedisMgr::GetInstance()->ExistsKey(request_json["user"].asString());
            if (b_usr_exist)
            {
                std::cout << " user exist" << std::endl;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserExist);
                std::string jsonstr = response_json.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }
            // 查找数据库判断用户是否存在
            // 5. 【核心修正】真正写入 MySQL
            int uid = MysqlMgr::GetInstance()->RegUser(
                request_json["user"].asString(), request_json["email"].asString(), request_json["passwd"].asString(),
                "" // icon 默认为空
            );

            // 如果 MySQL 返回 0 或 -1，说明用户名或邮箱已存在
            if (uid == 0 || uid == -1)
            {
                std::cout << "User or email exist in DB" << std::endl;
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserExist);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            // 6. 返回成功 (带上生成的 uid)
            std::cout << "Register Success, uid: " << uid << std::endl;
            response_json["error"] = 0;
            response_json["uid"] = uid; // 把 uid 给客户端
            response_json["email"] = request_json["email"];
            response_json["user"] = request_json["user"];
            // 不要返回密码
            beast::ostream(connection->_response.body()) << response_json.toStyledString();
            return true;
        });

    RegisterPost(
        "/reset_pwd",
        [](std::shared_ptr<HttpConnection> connection)
        {
            auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            connection->_response.set(http::field::content_type, "text/json");
            Json::Value response_json;
            Json::Value request_json;
            Json::Reader reader;
            bool parse_success = reader.parse(body_str, request_json);
            if (!parse_success)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            std::string varify_code;
            bool b_get_varify = RedisMgr::GetInstance()->Get(request_json["email"].asString(), varify_code);
            if (!b_get_varify)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyExpired);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }
            if (varify_code != request_json["varifycode"].asString())
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::VarifyCodeErr);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            int uid = MysqlMgr::GetInstance()->ResetPwd(
                request_json["user"].asString(), request_json["email"].asString(), request_json["passwd"].asString());
            if (uid == 0)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::EmailNotMatch);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }
            if (uid == -1)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::PasswdUpFailed);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
            response_json["uid"] = uid;
            response_json["email"] = request_json["email"];
            response_json["user"] = request_json["user"];
            beast::ostream(connection->_response.body()) << response_json.toStyledString();
            return true;
        });

    RegisterPost(
        "/user_login",
        [](std::shared_ptr<HttpConnection> connection)
        {
            auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            connection->_response.set(http::field::content_type, "text/json");
            Json::Value response_json;
            Json::Value request_json;
            Json::Reader reader;
            bool parse_success = reader.parse(body_str, request_json);
            if (!parse_success)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Error_Json);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            auto name = request_json["user"].asString();
            auto pwd = request_json["passwd"].asString();
            UserInfo userInfo;
            bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(name, pwd, userInfo);
            if (!pwd_valid)
            {
                if (userInfo.uid == 0)
                {
                    response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserNotExist);
                }
                else
                {
                    response_json["error"] = static_cast<int>(ChatApp::ErrorCode::PasswdErr);
                }
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            GetChatServerRsp reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
            if (reply.error() != static_cast<int>(ChatApp::ErrorCode::Success))
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::RPCGetFailed);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
            response_json["user"] = name;
            response_json["uid"] = userInfo.uid;
            response_json["token"] = reply.token();
            response_json["host"] = reply.host();
            response_json["port"] = reply.port();
            beast::ostream(connection->_response.body()) << response_json.toStyledString();
            return true;
        });
}

void LogicSystem::RegisterGet(std::string url, HttpHandler handler)
{
    _registerGet.emplace(url, handler);
}

void LogicSystem::RegisterPost(std::string url, HttpHandler handler)
{
    _registerPost.emplace(url, handler);
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> connection)
{
    if (_registerGet.find(path) != _registerGet.end())
    {
        _registerGet[path](connection);
        return true;
    }
    else
    {
        return false;
    }
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> connection)
{
    if (_registerPost.find(path) != _registerPost.end())
    {
        _registerPost[path](connection);
        return true;
    }
    else
    {
        return false;
    }
}

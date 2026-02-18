/**
 * @file    LogicSystem.cpp
 * @brief   业务逻辑分发系统实现
 * @author  msr
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
    // 注册 /get_test 路由
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

    // 注册 /get_varifycode 路由
    RegisterPost(
        "/get_varifycode",
        [](std::shared_ptr<HttpConnection> connection)
        {
            /**
             * @note [Memory Copy]
             * buffers_to_string 会将分散在 buffer 中的数据拷贝并拼接成一个 std::string。
             */
            auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
            std::cout << "receive body is " << body_str << std::endl;

            connection->_response.set(http::field::content_type, "text/json");
            Json::Value response_json;
            Json::Value request_json;
            Json::Reader reader;

            /**
             * @warning [PERFORMANCE BOTTLENECK] (性能瓶颈)
             * 此处的 JSON 解析是在主 IO 线程 (Reactor Thread) 中同步执行的。
             * 1. 这是一个 CPU 密集型操作。
             * 2. 如果 JSON 数据很大，或者并发很高，会阻塞 io_context，导致无法处理其他 socket 的 accept/read。
             * 3. 架构建议：将此类业务逻辑投递 (post) 到 Worker 线程池中处理。
             */
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
            
            // 将验证码写入 Redis (无 TTL，符合“一直有效”的需求)
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
            // 验证码校验 (走 Mock Redis)
            std::string varify_code;
            bool b_get_varify = RedisMgr::GetInstance()->Get(request_json["email"].asString(), varify_code);
            if (!b_get_varify)
            {
                // Mock 版其实不会进这里，因为 Get 永远返回 true
                std::cout << " get varify code expired" << std::endl;
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
            int uid = MysqlMgr::GetInstance()->LoginUser(name, pwd);
            if (uid == 0)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::UserNotExist);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }
            if (uid == -1)
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::PasswdErr);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            GetChatServerRsp reply = StatusGrpcClient::GetInstance()->GetChatServer(uid);
            if (reply.error() != static_cast<int>(ChatApp::ErrorCode::Success))
            {
                response_json["error"] = static_cast<int>(ChatApp::ErrorCode::RPCGetFailed);
                beast::ostream(connection->_response.body()) << response_json.toStyledString();
                return true;
            }

            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
            response_json["user"] = name;
            response_json["uid"] = uid;
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

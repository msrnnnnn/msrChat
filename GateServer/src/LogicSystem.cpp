#include "LogicSystem.h"
#include "HttpConnection.h"
#include "const.h"
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

LogicSystem::LogicSystem()
{
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

    RegisterPost(
        "/get_varifycode",
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
            auto email = request_json["email"].asString();
            std::cout << "email is " << email << std::endl;
            response_json["error"] = static_cast<int>(ChatApp::ErrorCode::Success);
            response_json["email"] = request_json["email"];
            std::string jsonstr = response_json.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
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
        return false;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> connection)
{
    if (_registerPost.find(path) != _registerPost.end())
    {
        _registerPost[path](connection);
        return true;
    }
    else
        return false;
}
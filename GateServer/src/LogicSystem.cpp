#include "LogicSystem.h"
#include "HttpConnection.h"

LogicSystem::LogicSystem()
{
    RegiterGet(
        "/get_test", [](std::shared_ptr<HttpConnection> conn)
        { beast::ostream(conn->_response.body()) << "receive get_test request"; });
}

void LogicSystem::RegiterGet(std::string url, HttpHandler handler)
{
    _registerGet.emplace(url, handler);
}

bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> conn)
{
    if (_registerGet.find(path) != _registerGet.end())
    {
        _registerGet[path](conn);
        return true;
    }
    else
        return false;
}
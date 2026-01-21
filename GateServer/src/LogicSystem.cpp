#include "LogicSystem.h"
#include "HttpConnection.h"

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
}

void LogicSystem::RegisterGet(std::string url, HttpHandler handler)
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
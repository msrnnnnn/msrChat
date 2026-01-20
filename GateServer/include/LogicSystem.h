#include "Singleton.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;
class LogicSystem : public Singleton<LogicSystem>
{
public:
    ~LogicSystem();
    void HandleGet(std::string, std::shared_ptr<HttpConnection>);
    void RegiterGet(std::string, HttpHandler HttpHandler);

private:
    LogicSystem() = default;
    std::map<std::string, HttpHandler> _handleGet;
    std::map<std::string, HttpHandler> _regiterGet;
};
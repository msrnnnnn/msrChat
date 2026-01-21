#include "Singleton.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;
class LogicSystem : public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;

public:
    ~LogicSystem() = default;
    bool HandleGet(std::string path, std::shared_ptr<HttpConnection> connection);
    void RegisterGet(std::string url, HttpHandler handler);
    bool HandlePost(std::string path, std::shared_ptr<HttpConnection> connection);
    void RegisterPost(std::string url, HttpHandler handler);

private:
    LogicSystem();
    std::unordered_map<std::string, HttpHandler> _registerPost;
    std::unordered_map<std::string, HttpHandler> _registerGet;
};
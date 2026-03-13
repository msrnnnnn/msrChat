#ifndef CSERVER_H
#define CSERVER_H

#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// 前向声明 CSession 类
class CSession;

// 路由处理函数类型定义
using RouteHandler = std::function<void(CSession *)>;

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    static inline std::atomic<uint64_t> s_session_id_allocator{1};

    // 构造函数
    CServer(boost::asio::io_context &io_context, short port);

    // 启动服务器
    void Start();

    // 添加用户会话映射
    void AddUserSession(int uid, std::shared_ptr<CSession> session);

    // 移除用户会话映射
    void RemoveUserSession(int uid);

    // 根据UUID清除会话
    void ClearSession(const std::string &uuid);

    // 转发消息到指定UID
    bool ForwardMessage(int target_uid, const std::string &msg_data);
    void StoreOfflineMessage(int target_uid, const std::string &msg_data);
    void SendOfflineMessages(int uid, std::shared_ptr<CSession> session);
    bool ValidateToken(int uid, const std::string &token);

    void SetAuthServer(const std::string &host, const std::string &port);

private:
    // 接受新连接
    void DoAccept();

    // UID到会话的映射表
    std::unordered_map<int, std::shared_ptr<CSession>> _uid_sessions;
    std::mutex _session_mtx;

    // UUID到会话的映射（用于管理连接）
    std::unordered_map<std::string, std::shared_ptr<CSession>> _uuid_sessions;
    std::mutex _uuid_session_mtx;

    // Boost.Asio 相关
    boost::asio::io_context &_io_context;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::string _auth_host;
    std::string _auth_port;
};

#endif // CSERVER_H

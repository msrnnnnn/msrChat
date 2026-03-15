/**
 * @file CServer.h
 * @brief 聊天服务 TCP 入口类
 * @details 负责连接接入、会话管理、消息转发与离线消息处理。
 */
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
    /**
     * @brief 会话 ID 自增分配器
     */
    static inline std::atomic<uint64_t> s_session_id_allocator{1};

    /**
     * @brief 构造函数
     * @param io_context io 上下文
     * @param port 监听端口
     */
    CServer(boost::asio::io_context &io_context, short port);

    /**
     * @brief 启动服务器并开始异步接入
     */
    void Start();

    /**
     * @brief 添加 UID 到会话映射
     * @param uid 用户 UID
     * @param session 会话对象
     */
    void AddUserSession(int uid, std::shared_ptr<CSession> session);

    /**
     * @brief 移除 UID 到会话映射
     * @param uid 用户 UID
     */
    void RemoveUserSession(int uid);

    /**
     * @brief 根据 UUID 清除会话
     * @param uuid 会话 UUID
     */
    void ClearSession(const std::string &uuid);

    /**
     * @brief 转发消息到在线用户
     * @param target_uid 目标用户 UID
     * @param msg_data 消息内容
     * @return true 转发成功，false 目标离线
     */
    bool ForwardMessage(int target_uid, const std::string &msg_data);
    /**
     * @brief 存储离线消息
     * @param target_uid 目标用户 UID
     * @param msg_data 消息内容
     */
    void StoreOfflineMessage(int target_uid, const std::string &msg_data);
    /**
     * @brief 发送并清空用户离线消息
     * @param uid 用户 UID
     * @param session 当前会话
     */
    void SendOfflineMessages(int uid, std::shared_ptr<CSession> session);
    /**
     * @brief 验证登录 Token
     * @param uid 用户 UID
     * @param token 用户 Token
     * @return true 验证通过
     */
    bool ValidateToken(int uid, const std::string &token);

    /**
     * @brief 设置认证服务地址
     * @param host 认证服务主机
     * @param port 认证服务端口
     */
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

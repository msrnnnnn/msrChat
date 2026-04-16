/**
 * @file CServer.h
 * @brief 聊天服务 TCP 入口类
 * @details 负责连接接入，委托会话管理、消息路由、令牌验证等到专用管理器。
 */
#ifndef CSERVER_H
#define CSERVER_H

#include "SessionManager.h"
#include "TokenManager.h"
#include "ThreadPool.h"
#include <boost/asio.hpp>
#include <memory>
#include <string>

class CSession;

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    static inline std::atomic<uint64_t> s_session_id_allocator{1};

    CServer(boost::asio::io_context &io_context, short port);
    ~CServer();

    void Start();

    void AddUserSession(int uid, std::shared_ptr<CSession> session);
    void RemoveUserSession(int uid);
    void ClearSession(const std::string &uuid);

    bool ForwardMessage(int target_uid, const std::string &msg_data);
    void StoreOfflineMessage(int target_uid, const std::string &msg_data);
    void SendOfflineMessages(int uid, std::shared_ptr<CSession> session);

    void SetToken(int uid, const std::string &token);
    bool CheckToken(int uid, const std::string &token);
    void RemoveToken(int uid);

    ThreadPool &GetThreadPool()
    {
        return _thread_pool;
    }

    void Stop();

private:
    void DoAccept();

    boost::asio::io_context &_io_context;
    boost::asio::ip::tcp::acceptor _acceptor;
    ThreadPool _thread_pool;
    std::atomic<bool> _stopped{false};
};

#endif // CSERVER_H

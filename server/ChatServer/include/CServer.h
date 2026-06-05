/**
 * @file CServer.h
 * @brief 聊天服务 TCP 入口类
 * @details 负责连接接入，委托会话管理、消息路由、令牌验证等到专用管理器。
 */
#ifndef CSERVER_H
#define CSERVER_H

#include "ThreadPool.h"
#include <boost/asio.hpp>
#include <memory>
#include <string>

struct ChatMessage;
class CSession;

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    static inline std::atomic<uint64_t> s_session_id_allocator{1};

    CServer(boost::asio::io_context &io_context, short port);
    ~CServer();

    void Start();

    bool ForwardRawMessage(int target_uid, uint16_t msg_id, const std::string &body_data);
    bool StoreOfflineMessage(int target_uid, const std::string &msg_data);
    bool StoreOfflineMessage(const ChatMessage &msg);  // Phase D: 图片消息直接传 ChatMessage
    void SendOfflineMessages(int uid, std::shared_ptr<CSession> session);

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

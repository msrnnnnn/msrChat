/**
 * @file CServer.h
 * @brief 聊天服务 TCP 入口类
 * @details 负责连接接入、会话管理、消息转发与离线消息处理。
 */
#ifndef CSERVER_H
#define CSERVER_H

#include "ShardedMap.h"
#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>

class CSession;

class CServer : public std::enable_shared_from_this<CServer>
{
public:
    static inline std::atomic<uint64_t> s_session_id_allocator{1};

    CServer(boost::asio::io_context &io_context, short port);

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

private:
    void DoAccept();

    ShardedMap<int, std::shared_ptr<CSession>> _uid_sessions{32};
    ShardedMap<std::string, std::shared_ptr<CSession>> _uuid_sessions{32};
    ShardedMap<int, std::string> _uid_tokens{32};

    boost::asio::io_context &_io_context;
    boost::asio::ip::tcp::acceptor _acceptor;
};

#endif // CSERVER_H

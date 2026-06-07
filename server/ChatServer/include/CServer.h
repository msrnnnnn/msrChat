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

    /**
     * @brief 启动服务端，开始监听连接
     */
    void Start();

    /**
     * @brief 转发原始消息到在线目标用户
     * @param target_uid 目标用户 UID
     * @param msg_id 消息类型 ID
     * @param body_data 序列化后的消息体（protobuf）
     * @return 是否成功转发
     */
    bool ForwardRawMessage(int target_uid, uint16_t msg_id, const std::string &body_data);

    /**
     * @brief 将消息存入离线队列（原始数据形式）
     * @param target_uid 目标用户 UID
     * @param msg_data 序列化后的消息数据
     * @return 是否存储成功
     */
    bool StoreOfflineMessage(int target_uid, const std::string &msg_data);

    /**
     * @brief 将 ChatMessage 存入离线队列
     * @param msg 聊天消息结构体（含图片等完整信息）
     * @return 是否存储成功
     */
    bool StoreOfflineMessage(const ChatMessage &msg);

    /**
     * @brief 向已登录的会话推送离线消息
     * @param uid 用户 UID
     * @param session 目标会话
     */
    void SendOfflineMessages(int uid, const std::shared_ptr<CSession> &session);

    /**
     * @brief 向已登录的会话推送未送达的撤回通知
     * @param uid 用户 UID
     * @param session 目标会话
     */
    void FlushRecallNotifies(int uid, const std::shared_ptr<CSession> &session);

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

#pragma once
/**
 * @file MessageRouter.h
 * @brief 消息路由 —— 将消息转发到目标用户的会话
 * @details 通过 SessionManager 查找目标会话，如果在线则直接发送；
 *          如果不在线则写入离线消息队列。每条服务端生成的消息附带全局递增的 msg_id。
 */
#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "SessionManager.h"
#include <atomic>
#include <memory>
#include <string>

class CSession;

/**
 * @brief 消息路由器（单例）
 * @details 负责将服务端产生的消息（如系统通知、转发消息）路由到目标用户会话。
 *          同时维护全局递增的消息 ID 计数器 `_next_server_msg_id`。
 */
class MessageRouter
{
public:
    static MessageRouter &Instance()
    {
        static MessageRouter instance;
        return instance;
    }

    /**
     * @brief 转发消息到目标用户
     * @param target_uid 目标用户 ID
     * @param msg_data 已序列化的消息数据
     * @return true 目标在线且发送成功；false 目标不在线（消息已写入离线队列）
     */
    bool ForwardMessage(int target_uid, const std::string &msg_data);

    /**
     * @brief 向指定会话直接发送消息
     * @param session 目标会话
     * @param msg_data 已序列化的消息数据
     * @param msg_id 消息 ID
     * @return true 发送成功
     */
    bool SendToSession(const std::shared_ptr<CSession> &session, const std::string &msg_data, short msg_id);

private:
    MessageRouter() = default;
    ~MessageRouter() = default;

    MessageRouter(const MessageRouter &) = delete;
    MessageRouter &operator=(const MessageRouter &) = delete;

    MessageRouter(MessageRouter &&) = delete;
    MessageRouter &operator=(MessageRouter &&) = delete;

    static std::atomic<int64_t> _next_server_msg_id;
};

#endif

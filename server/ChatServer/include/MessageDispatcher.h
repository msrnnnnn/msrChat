#pragma once
/**
 * @file MessageDispatcher.h
 * @brief 消息分发器 —— 根据 msg_id 查找注册的处理器并调用
 * @details 采用单例模式，启动时注册所有消息处理器。分发时先检查是否需要鉴权，
 *          若需要且 session 未登录则拒绝分发。
 */
#ifndef MESSAGEDISPATCHER_H
#define MESSAGEDISPATCHER_H
#include "const.h"
#include "CSession.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

using MessageHandler = std::function<bool(CSession &session, const std::string &body_data)>;

/**
 * @brief 消息处理器注册信息
 */
struct HandlerInfo
{
    MessageHandler handler;      ///< 处理函数
    bool requires_auth;          ///< 是否需要用户已登录
};

/**
 * @brief 消息分发器（单例）
 * @details 线程安全：构造时在单线程中注册完所有处理器，之后只读访问 `_handlers`。
 *          分发操作本身是 const 的，允许多线程并发读取。
 */
class MessageDispatcher
{
public:
    static MessageDispatcher &Instance()
    {
        static MessageDispatcher instance;
        return instance;
    }

    void RegisterHandler(uint16_t msg_id, MessageHandler handler, bool requires_auth = false)
    {
        _handlers[msg_id] = {std::move(handler), requires_auth};
    }

    bool Dispatch(CSession &session, uint16_t msg_id, const std::string &body_data) const
    {
        auto it = _handlers.find(msg_id);
        if (it == _handlers.end())
        {
            return false;
        }

        const auto &info = it->second;
        if (info.requires_auth && session.GetUserUid() <= 0)
        {
            return false;
        }

        return info.handler(session, body_data);
    }

private:
    MessageDispatcher()
    {
        RegisterDefaultHandlers();
    }

    MessageDispatcher(const MessageDispatcher &) = delete;
    MessageDispatcher &operator=(const MessageDispatcher &) = delete;

    void RegisterDefaultHandlers();

    std::unordered_map<uint16_t, HandlerInfo> _handlers;
};

#endif // MESSAGEDISPATCHER_H

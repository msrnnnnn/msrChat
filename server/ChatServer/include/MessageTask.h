#pragma once
/**
 * @file MessageTask.h
 * @brief 消息任务封装
 * @details 将从网络层接收的原始数据封装成任务，交给工作线程处理
 */
#ifndef MESSAGE_TASK_H
#define MESSAGE_TASK_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

class CSession;

struct MessageTask
{
    std::weak_ptr<CSession> session;
    uint16_t msg_id;
    std::string body_data;

    MessageTask()
        : msg_id(0)
    {
    }

    MessageTask(std::weak_ptr<CSession> sess, uint16_t id, std::string data)
        : session(sess), msg_id(id), body_data(std::move(data))
    {
    }

    MessageTask(const MessageTask &) = delete;
    MessageTask &operator=(const MessageTask &) = delete;

    MessageTask(MessageTask &&other) noexcept
        : session(std::move(other.session)),
          msg_id(other.msg_id),
          body_data(std::move(other.body_data))
    {
    }

    MessageTask &operator=(MessageTask &&other) noexcept
    {
        if (this != &other)
        {
            session = std::move(other.session);
            msg_id = other.msg_id;
            body_data = std::move(other.body_data);
        }
        return *this;
    }

    /**
     * @brief 检查任务是否有效（会话未断开且 msg_id 非零）
     */
    bool IsValid() const
    {
        return !session.expired() && msg_id != 0;
    }

    /**
     * @brief 尝试锁定会话 shared_ptr（仅当会话仍存活时成功）
     * @return 会话 shared_ptr，若已失效则返回 nullptr
     */
    std::shared_ptr<CSession> LockSession() const
    {
        return session.lock();
    }
};

#endif // MESSAGE_TASK_H

/**
 * @file LogicSystem.h
 * @brief 业务逻辑处理系统
 * @details 解耦 I/O 与业务逻辑，负责消息的解析和业务分发
 */
#ifndef LOGIC_SYSTEM_H
#define LOGIC_SYSTEM_H

#include "CSingleton.h"
#include "MessageTask.h"
#include "ThreadPool.h"
#include <atomic>
#include <boost/asio.hpp>
#include <memory>

class CSession;

class LogicSystem : public CSingleton<LogicSystem>
{
    friend class CSingleton<LogicSystem>;

public:
    void PostTask(MessageTask task);

    void SetIOContext(boost::asio::io_context *ioc);

    void Shutdown();
    bool IsShuttingDown() const;

    size_t GetQueueSize() const;

private:
    LogicSystem();
    ~LogicSystem();

    LogicSystem(const LogicSystem &) = delete;
    LogicSystem &operator=(const LogicSystem &) = delete;

    void ProcessTask(MessageTask task);

    boost::asio::io_context *_ioc = nullptr;
    ThreadPool _thread_pool;

    std::atomic<bool> _shutting_down{false};

    static constexpr size_t DEFAULT_THREAD_NUM = 4;
};

#endif // LOGIC_SYSTEM_H

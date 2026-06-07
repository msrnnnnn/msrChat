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
#include <memory>

class CSession;

class LogicSystem : public CSingleton<LogicSystem>
{
    friend class CSingleton<LogicSystem>;

public:
    /**
     * @brief 将业务任务投递到线程池异步执行
     * @param task 封装了消息与会话的 MessageTask
     * @details 任务由线程池中的工作线程消费，调用 ProcessTask 处理
     */
    void PostTask(MessageTask task);

    /**
     * @brief 关闭逻辑系统，停止接收新任务
     */
    void Shutdown();

private:
    LogicSystem();
    ~LogicSystem();

    LogicSystem(const LogicSystem &) = delete;
    LogicSystem &operator=(const LogicSystem &) = delete;

    /**
     * @brief 处理单个业务任务（在工作线程中执行）
     * @param task 封装了消息与会话的 MessageTask
     * @details 根据消息 ID 分发到对应的业务处理函数
     */
    void ProcessTask(MessageTask task);

    ThreadPool _thread_pool;

    std::atomic<bool> _shutting_down{false};

    static constexpr size_t DEFAULT_THREAD_NUM = 4;
};

#endif // LOGIC_SYSTEM_H

/**
 * @file LogicSystem.cpp
 * @brief 业务逻辑处理系统实现
 */
#include "LogicSystem.h"
#include "CSession.h"
#include "MessageDispatcher.h"
#include <spdlog/spdlog.h>

/**
 * @brief 构造函数，初始化工作线程池
 */
LogicSystem::LogicSystem()
    : _thread_pool(DEFAULT_THREAD_NUM)
{
    spdlog::info("[LogicSystem] Initialized with {} worker threads", DEFAULT_THREAD_NUM);
}

/**
 * @brief 析构函数，确保线程池安全退出
 */
LogicSystem::~LogicSystem()
{
    Shutdown();
}

/**
 * @brief 投递消息任务到处理队列
 * @param task 消息任务（含会话、消息 ID、数据）
 * @details 队列满时返回 ERR_BUSY 给客户端
 */
void LogicSystem::PostTask(MessageTask task)
{
    if (_shutting_down.load())
    {
        spdlog::warn("[LogicSystem] System is shutting down, reject new task");
        return;
    }

    if (!task.IsValid())
    {
        spdlog::error("[LogicSystem] Invalid task received");
        return;
    }

    auto session = task.LockSession();
    if (!session)
    {
        spdlog::warn("[LogicSystem] Session expired before enqueue");
        return;
    }

    auto self = this;
    auto shared_task = std::make_shared<MessageTask>(std::move(task));
    if (!_thread_pool.Enqueue([self, shared_task]() { self->ProcessTask(std::move(*shared_task)); }))
    {
        spdlog::warn("[LogicSystem] Queue full, rejecting msg_id={} for session={}", 
                     shared_task->msg_id, session->GetUuid());
        session->ContinueReading();
    }
}

/**
 * @brief 处理消息任务
 * @param task 消息任务
 * @details 先查本地 handler，查不到则转发给 MessageDispatcher
 */
void LogicSystem::ProcessTask(MessageTask task)
{
    auto session = task.LockSession();
    if (!session)
    {
        spdlog::warn("[LogicSystem] Session expired for msg_id {}", task.msg_id);
        return;
    }

    if (session->IsClosed())
    {
        spdlog::warn("[LogicSystem] Session {} is closed, skip msg_id {}", session->GetUuid(), task.msg_id);
        return;
    }

    spdlog::debug("[LogicSystem] Processing msg_id {} for session {}", task.msg_id, session->GetUuid());

    bool handled = MessageDispatcher::Instance().Dispatch(*session, task.msg_id, task.body_data);
    if (!handled)
    {
        spdlog::warn("[LogicSystem] No handler found for msg_id {}", task.msg_id);
        session->ContinueReading();
    }
}

/**
 * @brief 关闭逻辑处理系统
 * @details 原子标记防止重复关闭，设置标志后线程池停止接受新任务并等待已入队任务完成
 */
void LogicSystem::Shutdown()
{
    bool expected = false;
    if (!_shutting_down.compare_exchange_strong(expected, true))
    {
        spdlog::warn("[LogicSystem] Already shutting down");
        return;
    }

    spdlog::info("[LogicSystem] Shutting down...");
    _thread_pool.Shutdown();
    spdlog::info("[LogicSystem] Shutdown complete");
}


/**
 * @file LogicSystem.cpp
 * @brief 业务逻辑处理系统实现
 */
#include "LogicSystem.h"
#include "CSession.h"
#include "MessageDispatcher.h"
#include <spdlog/spdlog.h>

LogicSystem::LogicSystem()
    : _thread_pool(DEFAULT_THREAD_NUM)
{
    spdlog::info("[LogicSystem] Initialized with {} worker threads", DEFAULT_THREAD_NUM);
}

LogicSystem::~LogicSystem()
{
    Shutdown();
}

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

    auto self = this;
    auto shared_task = std::make_shared<MessageTask>(std::move(task));
    _thread_pool.Enqueue([self, shared_task]() { self->ProcessTask(std::move(*shared_task)); });
}

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

    BusinessHandler handler;
    auto it = _handlers.find(task.msg_id);
    if (it != _handlers.end())
    {
        handler = it->second;
    }

    if (handler)
    {
        try
        {
            handler(session, task.body_data);
            return;
        }
        catch (const std::exception &e)
        {
            spdlog::error("[LogicSystem] Handler exception for msg_id {}: {}", task.msg_id, e.what());
        }
    }

    spdlog::debug("[LogicSystem] Delegating msg_id {} to MessageDispatcher", task.msg_id);

    bool handled = MessageDispatcher::Instance().Dispatch(*session, task.msg_id, task.body_data);
    if (!handled)
    {
        spdlog::warn("[LogicSystem] No handler found for msg_id {}", task.msg_id);
        session->Send(task.body_data, task.msg_id);
        session->ContinueReading();
    }
}

void LogicSystem::RegisterHandler(uint16_t msg_id, BusinessHandler handler)
{
    _handlers[msg_id] = std::move(handler);
    spdlog::info("[LogicSystem] Registered handler for msg_id {}", msg_id);
}

void LogicSystem::RemoveHandler(uint16_t msg_id)
{
    _handlers.erase(msg_id);
    spdlog::info("[LogicSystem] Removed handler for msg_id {}", msg_id);
}

void LogicSystem::SetIOContext(boost::asio::io_context *ioc)
{
    _ioc = ioc;
}

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

bool LogicSystem::IsShuttingDown() const
{
    return _shutting_down.load();
}

size_t LogicSystem::GetQueueSize() const
{
    return _thread_pool.GetTaskCount();
}

size_t LogicSystem::GetHandlerCount() const
{
    return _handlers.size();
}

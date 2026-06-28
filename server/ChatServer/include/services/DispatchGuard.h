#pragma once
/**
 * @file DispatchGuard.h
 * @brief RAII 守卫 —— 确保 handler 退出时调用 ContinueReading
 * @details 构造时绑定 session，析构时自动调用 ContinueReading()。
 *          异步 handler 在 Enqueue 后应调用 Release() 解除绑定，
 *          由异步 lambda 自行负责 ContinueReading。
 */

#include "CSession.h"

class DispatchGuard
{
public:
    explicit DispatchGuard(CSession &session) : _session(&session), _released(false)
    {
    }

    ~DispatchGuard()
    {
        if (!_released && _session)
        {
            _session->ContinueReading();
        }
    }

    /// 解除绑定（异步 handler 在 Enqueue 后调用）
    void Release()
    {
        _released = true;
    }

    DispatchGuard(const DispatchGuard &) = delete;
    DispatchGuard &operator=(const DispatchGuard &) = delete;
    DispatchGuard(DispatchGuard &&) = delete;
    DispatchGuard &operator=(DispatchGuard &&) = delete;

private:
    CSession *_session;
    bool _released;
};

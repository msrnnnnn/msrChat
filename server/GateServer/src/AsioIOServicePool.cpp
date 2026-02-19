/**
 * @file    AsioIOServicePool.cpp
 * @brief   IO 上下文线程池实现
 */

#include "AsioIOServicePool.h"
#include <iostream>
#include <memory>

AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : nextIOService_(0)
{
    if (size == 0)
    {
        size = 2;
    }
    for (std::size_t i = 0; i < size; ++i)
    {
        ioServices_.emplace_back(std::make_shared<IOService>());
        // work_guard 确保 io_context 不会因为没有任务而退出，
        // 从而保持线程持续运行等待新的异步事件。
        works_.emplace_back(boost::asio::make_work_guard(ioServices_[i]->get_executor()));
        threads_.emplace_back([this, i]() { ioServices_[i]->run(); });
    }
}

AsioIOServicePool::~AsioIOServicePool()
{
    Stop();
}

std::shared_ptr<AsioIOServicePool::IOService> AsioIOServicePool::GetIOService()
{
    // 使用原子操作实现的 Round-Robin (轮询) 调度策略，
    // 将连接均匀分发到不同的 IO 线程，实现负载均衡。
    std::size_t index = nextIOService_.fetch_add(1, std::memory_order_relaxed);
    return ioServices_[index % ioServices_.size()];
}

void AsioIOServicePool::Stop()
{
    for (auto &work : works_)
    {
        work.reset();
    }
    for (auto &service : ioServices_)
    {
        service->stop();
    }
    for (auto &t : threads_)
    {
        if(t.joinable()) {
            t.join();
        }
    }
}

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
    // 轮询分配 IO Context
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

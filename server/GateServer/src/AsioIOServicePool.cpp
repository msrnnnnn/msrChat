/**
 * @file    AsioIOServicePool.cpp
 * @brief   IO 上下文线程池实现
 * @author  msr
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
    std::cout << "AsioIOServicePool destruct" << std::endl;
}

std::shared_ptr<AsioIOServicePool::IOService> AsioIOServicePool::GetIOService()
{
    // 1. fetch_add 返回的是"加之前"的值 (old_value)
    // 2. memory_order_relaxed 告诉 CPU：别搞屏障，就要最快的原子加法
    std::size_t index = nextIOService_.fetch_add(1, std::memory_order_relaxed);
    return ioServices_[index % ioServices_.size()];
}

void AsioIOServicePool::Stop()
{
    // 因为仅仅执行work.reset并不能让iocontext从run的状态中退出
    // 当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
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
        t.join();
    }
}

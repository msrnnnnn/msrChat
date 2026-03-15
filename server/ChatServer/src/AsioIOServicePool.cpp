/**
 * @file AsioIOServicePool.cpp
 * @brief Boost.Asio I/O 线程池实现
 * @details 负责创建 io_context 列表、工作守护对象和工作线程。
 */
#include "AsioIOServicePool.h"
#include <iostream>

/**
 * @brief 构造函数
 * @param size 线程池大小
 */
AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : nextIOService_(0)
{
    if (size == 0)
        size = 2;
    for (std::size_t i = 0; i < size; ++i)
    {
        ioServices_.emplace_back(std::make_shared<IOService>());
        works_.emplace_back(boost::asio::make_work_guard(ioServices_[i]->get_executor()));
        threads_.emplace_back([this, i]() { ioServices_[i]->run(); });
    }
}

/**
 * @brief 析构函数
 */
AsioIOServicePool::~AsioIOServicePool()
{
    Stop();
}

/**
 * @brief 获取一个 io_context
 * @return boost::asio::io_context& 轮询分配的 io_context
 */
boost::asio::io_context &AsioIOServicePool::GetIOService()
{
    std::size_t index = nextIOService_.fetch_add(1, std::memory_order_relaxed);
    return *(ioServices_[index % ioServices_.size()]);
}

/**
 * @brief 停止线程池与所有 I/O 服务
 */
void AsioIOServicePool::Stop()
{
    for (auto &work : works_)
        work.reset();
    for (auto &service : ioServices_)
        service->stop();
    for (auto &t : threads_)
    {
        if (t.joinable())
            t.join();
    }
}

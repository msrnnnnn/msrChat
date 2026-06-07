/**
 * @file AsioIOServicePool.cpp
 * @brief Boost.Asio I/O 线程池实现
 * @details 负责创建 io_context 列表、工作守护对象和工作线程。
 */
#include "AsioIOServicePool.h"

/**
 * @brief 构造函数
 * @param size 线程池大小
 */
AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : nextIOService_(0)
{
    if (size == 0)
    {
        const std::size_t hardware_threads = std::thread::hardware_concurrency();
        size = std::max<std::size_t>(4, hardware_threads == 0 ? 4 : hardware_threads);
    }
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
    // 原子递增实现轮询负载均衡，relaxed 排序即可（仅需原子性）
    std::size_t index = nextIOService_.fetch_add(1, std::memory_order_relaxed);
    return *(ioServices_[index % ioServices_.size()]);
}

/**
 * @brief 停止线程池与所有 I/O 服务
 */
void AsioIOServicePool::Stop()
{
    // 先释放 work guard，让 io_context::run() 可以自然退出
    for (auto &work : works_)
        work.reset();
    // 再显式停止，唤醒所有阻塞在 run() 上的线程
    for (auto &service : ioServices_)
        service->stop();
    // 等待所有工作线程结束
    for (auto &t : threads_)
    {
        if (t.joinable())
            t.join();
    }
}

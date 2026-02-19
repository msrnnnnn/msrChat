/**
 * @file    AsioIOServicePool.h
 * @brief   IO 上下文线程池声明
 * @details 管理多个 io_context 和工作线程，通过 Round-Robin 方式分配 IO 任务。
 */
#pragma once

#include "Singleton.h"
#include <atomic>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>

/**
 * @class   AsioIOServicePool
 * @brief   IO 上下文线程池 (Singleton)
 * @details 负责创建和管理 IO 线程池，提供 IO 上下文分发。
 */
class AsioIOServicePool : public Singleton<AsioIOServicePool>
{
    friend Singleton<AsioIOServicePool>;

public:
    using IOService = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool &) = delete;
    AsioIOServicePool &operator=(const AsioIOServicePool &) = delete;

    /**
     * @brief   获取一个 IO 上下文
     * @details 使用 Round-Robin (轮询) 算法负载均衡。
     * @return  std::shared_ptr<IOService>
     */
    std::shared_ptr<IOService> GetIOService();

    /**
     * @brief   停止所有 IO 服务和线程
     */
    void Stop();

private:
    /**
     * @brief   构造函数
     * @param   size 线程池大小，默认为硬件并发数 (CPU 核数)
     */
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());

    std::vector<std::shared_ptr<IOService>> ioServices_;
    std::vector<WorkGuard> works_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> nextIOService_;
};

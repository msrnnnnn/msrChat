/**
 * @file AsioIOServicePool.h
 * @brief Boost.Asio I/O 线程池
 * @details 使用 executor_work_guard 和原子变量实现轮询的 I/O 服务池
 */

#ifndef ASIOIOSERVICEPOOL_H
#define ASIOIOSERVICEPOOL_H

#include "Singleton.h"
#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

/**
 * @class AsioIOServicePool
 * @brief I/O 线程池类
 * 
 * 使用 Boss-Worker 线程池模型，每个线程运行一个 io_context
 * 使用轮询算法分配 io_context 给新的连接
 */
class AsioIOServicePool : public Singleton<AsioIOServicePool> {
    friend class Singleton<AsioIOServicePool>;
    
public:
    using IOService = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    ~AsioIOServicePool();
    
    /**
     * @brief 获取一个 io_context
     * @return boost::asio::io_context& io_context 的引用
     * 
     * 使用轮询算法分配 io_context，确保负载均衡
     */
    boost::asio::io_context& GetIOService();
    
    /**
     * @brief 停止所有 I/O 服务
     */
    void Stop();

private:
    /**
     * @brief 构造函数
     * @param size 线程池大小，默认为 CPU 核心数
     */
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
    
    std::vector<std::shared_ptr<IOService>> ioServices_;  ///< I/O 服务列表
    std::vector<WorkGuard> works_;                        ///< 工作守卫，防止 io_context 退出
    std::vector<std::thread> threads_;                    ///< 工作线程列表
    std::atomic<std::size_t> nextIOService_;              ///< 下一个要分配的 I/O 服务索引
};

#endif // ASIOIOSERVICEPOOL_H
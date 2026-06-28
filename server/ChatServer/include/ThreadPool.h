#pragma once
/**
 * @file ThreadPool.h
 * @brief 线程池 —— 固定数量的工作线程 + 任务队列
 * @details 工作线程从任务队列中取任务执行，队列为空时阻塞等待。
 *          Shutdown() 唤醒所有线程并等待退出。
 */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

/**
 * @brief 线程池
 * @details 默认线程数 = `std::thread::hardware_concurrency()`。
 *          所有工作线程共享同一个任务队列，使用条件变量进行生产者-消费者协调。
 *
 *          项目中有两个独立的 ThreadPool 实例：
 *          - CServer::_thread_pool：网络 I/O 线程池，处理 Boost.Asio 异步操作（连接接入、数据收发）
 *          - LogicSystem::_thread_pool：业务逻辑线程池，处理消息解析和业务分发（Handler 执行）
 *          两者职责分离，避免业务逻辑阻塞网络 I/O。
 */
class ThreadPool
{
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t thread_num = std::thread::hardware_concurrency(), size_t max_queue_size = 10000);
    ~ThreadPool();

    /**
     * @brief 向任务队列提交一个任务
     * @param task 可调用对象（std::function<void()>）
     * @return 队列未满返回 true，队列满返回 false（拒绝入队）
     */
    bool Enqueue(Task task);

    /**
     * @brief 关闭线程池 —— 唤醒所有工作线程并等待退出
     */
    void Shutdown();

    /**
     * @brief 获取当前队列中的任务数量
     */
    size_t GetTaskCount() const;

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    void WorkerThread();

    std::vector<std::thread> _threads;
    std::queue<Task> _tasks;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
    std::atomic<size_t> _task_count;
    size_t _max_queue_size;
};

#endif

/**
 * @file ThreadPool.cpp
 * @brief 线程池实现
 * @details 预先创建工作线程，消费任务队列，支持安全关闭。
 */
#include "ThreadPool.h"
#include <spdlog/spdlog.h>
#include <utility>

/**
 * @brief 构造函数
 * @param thread_num 线程数量
 */
ThreadPool::ThreadPool(size_t thread_num)
    : _stop(false), _task_count(0)
{
    _threads.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i) {
        _threads.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

/**
 * @brief 析构函数
 */
ThreadPool::~ThreadPool()
{
    Shutdown();
}

/**
 * @brief 投递任务到队列
 * @param task 任务函数
 * @details 加锁入队后通知一个等待线程
 */
void ThreadPool::Enqueue(Task task)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.push(std::move(task));
        ++_task_count;
    }
    _cv.notify_one();
}

/**
 * @brief 关闭线程池
 * @details 设置停止标志并唤醒所有线程，等待它们结束
 */
void ThreadPool::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _stop = true;
    }
    _cv.notify_all();
    
    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

/**
 * @brief 工作线程主循环
 * @details 等待条件变量，有任务时出队执行；停止标志置起且队列空时退出
 */
void ThreadPool::WorkerThread()
{
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(_mutex);
             _cv.wait(lock, [this] {
                return _stop || !_tasks.empty();
            });
            
            // 停止且队列为空时退出工作线程
            if (_stop && _tasks.empty()) {
                return;
            }
            
            // 取出队首任务并递减计数器（锁内操作，线程安全）
            task = std::move(_tasks.front());
            _tasks.pop();
            --_task_count;
        }
        
        if (task) {
            try {
                task();
            } catch (const std::exception &e) {
                spdlog::error("[ThreadPool] Worker thread caught exception: {}", e.what());
            } catch (...) {
                spdlog::error("[ThreadPool] Worker thread caught unknown exception");
            }
        }
    }
}

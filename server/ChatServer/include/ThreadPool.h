#pragma once
/**
 * @file ThreadPool.h
 * @brief 线程池 —— 固定数量的工作线程 + 有界环形缓冲区任务队列
 * @details 使用 mutex + condition_variable 实现生产者-消费者模型。
 *          任务队列采用连续内存的环形缓冲区（非 std::queue），缓存友好。
 *          队列满时 Enqueue 返回 false（反压），不会动态扩容。
 *
 *          项目中有两个独立的 ThreadPool 实例：
 *          - CServer::_thread_pool：网络 I/O 线程池，处理 Boost.Asio 异步操作
 *          - LogicSystem::_thread_pool：业务逻辑线程池，处理消息解析和业务分发
 */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool
{
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t thread_num = std::thread::hardware_concurrency(), size_t max_queue_size = 10000);
    ~ThreadPool();

    /**
     * @brief 向任务队列提交一个任务
     * @param task 可调用对象（std::function<void()>）
     * @return 队列未满返回 true，队列满返回 false（反压，拒绝入队）
     *
     * 实现细节（生产者端）：
     *   1. 加锁后检查 _count >= _capacity，满则返回 false
     *   2. 将 task 移动到 _buffer[_tail]，_tail 环形推进
     *   3. 递增 _count，释放锁后 notify_one 唤醒一个消费者
     */
    bool Enqueue(Task task);

    /**
     * @brief 关闭线程池 —— 设置停止标志，唤醒所有线程，等待退出
     * @details 调用后所有 WorkerThread 在消费完剩余任务后退出。
     *          等待队列排空再退出，保证已提交的任务不丢失。
     */
    void Shutdown();

    /**
     * @brief 获取当前队列中的待处理任务数量
     */
    size_t GetTaskCount() const;

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    /**
     * @brief 工作线程主循环（消费者端）
     *
     * 伪代码：
     *   loop:
     *     lock → wait(_count > 0 || _stop)
     *     if _stop && _count == 0 → return（退出线程）
     *     取 _buffer[_head]，_head 环形推进，--_count
     *     unlock → 执行 task
     *
     * 关键点：
     *   - _cv.wait 使用 predicate 形式，自动处理虚假唤醒（spurious wakeup）
     *   - 任务在锁外执行，避免持锁时间过长
     *   - try-catch 捕获任务异常，防止单个任务崩溃整个线程池
     */
    void WorkerThread();

    std::vector<std::thread> _threads;

    // 环形缓冲区（连续内存，缓存友好）
    std::vector<Task> _buffer;
    size_t _head = 0;       // 消费位置索引
    size_t _tail = 0;       // 生产位置索引
    size_t _count = 0;      // 当前队列中的任务数
    size_t _capacity;       // 队列最大容量（固定，不扩容）

    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
    std::atomic<size_t> _task_count;
};

#endif

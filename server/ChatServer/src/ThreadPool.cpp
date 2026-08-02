/**
 * @file ThreadPool.cpp
 * @brief 线程池实现 —— 有界环形缓冲区 + 生产者-消费者模型
 *
 * 设计要点（面试高频）：
 *   1. 环形缓冲区 vs std::queue：
 *      - std::queue 底层是 std::deque，分段连续内存，每次扩容分配新块
 *      - 环形缓冲区是单块连续内存，CPU cache line 预取友好，无动态分配
 *   2. mutex + cond_var 协作：
 *      - 生产者：加锁 → 写入 → 解锁 → notify_one
 *      - 消费者：加锁 → wait(predicate) → 取任务 → 解锁 → 执行
 *      - 任务在锁外执行，避免持锁时间过长
 *   3. 虚假唤醒（spurious wakeup）：
 *      - cond_var 可能在没有 notify 的情况下唤醒（POSIX 允许）
 *      - 解决方案：使用 predicate 形式的 wait，唤醒后重新检查条件
 *   4. 反压（backpressure）：
 *      - 队列满时 Enqueue 返回 false，调用方决定丢弃或等待
 *      - 防止生产速度远大于消费速度时内存无限增长
 */
#include "ThreadPool.h"
#include <spdlog/spdlog.h>
#include <utility>

/**
 * @brief 构造函数
 * @param thread_num 工作线程数量，默认为硬件并发数
 * @param max_queue_size 环形缓冲区容量，队列满时拒绝新任务
 */
ThreadPool::ThreadPool(size_t thread_num, size_t max_queue_size)
    : _capacity(max_queue_size), _stop(false), _task_count(0)
{
    _buffer.resize(_capacity);
    _threads.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i)
    {
        _threads.emplace_back(&ThreadPool::WorkerThread, this);
    }
    spdlog::info("[ThreadPool] Started with {} threads, queue capacity {}", thread_num, _capacity);
}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

/**
 * @brief 生产者：向环形缓冲区提交任务
 *
 * 流程：
 *   1. lock_guard 加锁
 *   2. 检查 _count >= _capacity → 满，返回 false（反压）
 *   3. 将 task 移动到 _buffer[_tail]
 *   4. _tail = (_tail + 1) % _capacity（环形推进）
 *   5. ++_count, ++_task_count
 *   6. 解锁后 notify_one 唤醒一个等待的消费者
 */
bool ThreadPool::Enqueue(Task task)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_count >= _capacity)
        {
            spdlog::warn("[ThreadPool] Queue full ({} >= {}), rejecting task", _count, _capacity);
            return false;
        }
        _buffer[_tail] = std::move(task);
        _tail = (_tail + 1) % _capacity;
        ++_count;
        ++_task_count;
    }
    _cv.notify_one();
    return true;
}

size_t ThreadPool::GetTaskCount() const
{
    return _task_count.load();
}

/**
 * @brief 关闭线程池
 *
 * 流程：
 *   1. 加锁设置 _stop = true
 *   2. notify_all 唤醒所有阻塞在 wait 的线程
 *   3. join 所有工作线程（等待它们处理完剩余任务后退出）
 *
 * 注意：必须先设置 _stop 再 notify，否则线程可能错过通知。
 */
void ThreadPool::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _stop = true;
    }
    _cv.notify_all();

    for (auto &thread : _threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    spdlog::info("[ThreadPool] All worker threads joined");
}

/**
 * @brief 消费者：工作线程主循环
 *
 * 流程：
 *   loop:
 *     1. unique_lock 加锁
 *     2. _cv.wait(lock, predicate) — 阻塞等待直到 _stop || _count > 0
 *        - predicate 形式自动处理虚假唤醒：醒来后重新检查条件，不满足则继续等待
 *     3. 如果 _stop && _count == 0 → 退出线程（队列排空后才退出，保证任务不丢失）
 *     4. 取 _buffer[_head]，_head 环形推进，--_count
 *     5. 解锁后执行 task（锁外执行，不阻塞其他线程）
 *     6. try-catch 捕获异常，防止单个任务崩溃整个线程
 *
 * 面试要点：
 *   - 为什么用 wait(predicate) 而不是 while + wait?
 *     → predicate 形式等价于 while(!pred()) wait(lock)，但更简洁且不易出错
 *   - 为什么 _stop && _count == 0 才退出？
 *     → 保证已入队的任务都被执行完，不会因为 Shutdown 丢失任务
 *   - 为什么 task 在锁外执行？
 *     → 持锁执行任务会阻塞其他线程的 Enqueue/Dequeue，并发度降为 1
 */
void ThreadPool::WorkerThread()
{
    while (true)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(lock, [this] { return _stop || _count > 0; });

            // 停止标志置起且队列为空 → 退出工作线程
            if (_stop && _count == 0)
            {
                return;
            }

            // 从环形缓冲区头部取出任务
            task = std::move(_buffer[_head]);
            _head = (_head + 1) % _capacity;
            --_count;
            --_task_count;
        }

        // 锁外执行任务，不阻塞其他线程
        if (task)
        {
            try
            {
                task();
            }
            catch (const std::exception &e)
            {
                spdlog::error("[ThreadPool] Worker thread caught exception: {}", e.what());
            }
            catch (...)
            {
                spdlog::error("[ThreadPool] Worker thread caught unknown exception");
            }
        }
    }
}

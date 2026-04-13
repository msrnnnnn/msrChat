#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

class ThreadPool
{
public:
    using Task = std::function<void()>;
    
    explicit ThreadPool(size_t thread_num = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    void Enqueue(Task task);
    void Shutdown();
    size_t GetTaskCount() const;
    
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    void WorkerThread();
    
    std::vector<std::thread> _threads;
    std::queue<Task> _tasks;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
    std::atomic<size_t> _task_count;
};

#endif

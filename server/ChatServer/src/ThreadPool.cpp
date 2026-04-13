#include "ThreadPool.h"
#include <utility>

ThreadPool::ThreadPool(size_t thread_num)
    : _stop(false), _task_count(0)
{
    _threads.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i) {
        _threads.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::Enqueue(Task task)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.push(std::move(task));
        ++_task_count;
    }
    _cv.notify_one();
}

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

size_t ThreadPool::GetTaskCount() const
{
    return _task_count.load();
}

void ThreadPool::WorkerThread()
{
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(lock, [this] {
                return _stop || !_tasks.empty();
            });
            
            if (_stop && _tasks.empty()) {
                return;
            }
            
            task = std::move(_tasks.front());
            _tasks.pop();
        }
        
        if (task) {
            task();
            --_task_count;
        }
    }
}

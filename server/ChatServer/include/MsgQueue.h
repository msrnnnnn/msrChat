#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template <typename T>
class MsgQueue
{
public:
    explicit MsgQueue(size_t capacity = 1024)
        : _capacity(capacity),
          _stop(false)
    {
    }

    ~MsgQueue() = default;

    bool Push(T &&msg);
    std::optional<T> Pop();

    void Shutdown();
    bool IsFull() const;
    bool IsEmpty() const;
    size_t Size() const;
    size_t Capacity() const
    {
        return _capacity;
    }

    MsgQueue(const MsgQueue &) = delete;
    MsgQueue &operator=(const MsgQueue &) = delete;

private:
    size_t _capacity;
    std::atomic<bool> _stop;
    mutable std::mutex _mutex;
    std::queue<T> _queue;
};

template <typename T>
bool MsgQueue<T>::Push(T &&msg)
{
    if (_stop.load(std::memory_order_acquire))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if (_stop.load(std::memory_order_relaxed))
    {
        return false;
    }

    if (_queue.size() >= _capacity)
    {
        return false;
    }

    _queue.push(std::move(msg));
    return true;
}

template <typename T>
std::optional<T> MsgQueue<T>::Pop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_queue.empty())
    {
        return std::nullopt;
    }

    T data = std::move(_queue.front());
    _queue.pop();
    return data;
}

template <typename T>
void MsgQueue<T>::Shutdown()
{
    _stop.store(true, std::memory_order_release);
}

template <typename T>
bool MsgQueue<T>::IsFull() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.size() >= _capacity;
}

template <typename T>
bool MsgQueue<T>::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.empty();
}

template <typename T>
size_t MsgQueue<T>::Size() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.size();
}

#endif

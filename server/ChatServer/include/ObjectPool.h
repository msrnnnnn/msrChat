#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

template <typename T>
class ObjectPool
{
public:
    explicit ObjectPool(std::size_t initial_size = 10000, std::size_t expand_size = 1024)
        : _expand_size(expand_size == 0 ? 1 : expand_size)
    {
        Expand(initial_size);
    }

    ~ObjectPool()
    {
        for (auto *object : _all_objects)
        {
            delete object;
        }
    }

    template <typename... Args>
    std::shared_ptr<T> Acquire(Args &&...args)
    {
        T *object = nullptr;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_available_objects.empty())
            {
                ExpandUnlocked(_expand_size);
            }
            object = _available_objects.back();
            _available_objects.pop_back();
        }

        object->Reset(std::forward<Args>(args)...);
        return std::shared_ptr<T>(object, [this](T *ptr) { Release(ptr); });
    }

    void Release(T *object)
    {
        if (!object)
        {
            return;
        }

        object->Reset();
        std::lock_guard<std::mutex> lock(_mutex);
        _available_objects.push_back(object);
    }

private:
    void Expand(std::size_t count)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        ExpandUnlocked(count);
    }

    void ExpandUnlocked(std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            auto *object = new T();
            _all_objects.push_back(object);
            _available_objects.push_back(object);
        }
    }

    std::mutex _mutex;
    std::vector<T *> _all_objects;
    std::vector<T *> _available_objects;
    std::size_t _expand_size;
};

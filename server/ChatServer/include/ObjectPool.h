#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <memory>
#include <mutex>
#include <vector>

template<typename T>
class ObjectPool
{
public:
    explicit ObjectPool(std::size_t initial_size = 1000, std::size_t grow_size = 1024)
        : _grow_size(grow_size)
    {
        for (std::size_t i = 0; i < initial_size; ++i)
        {
            auto *obj = new T();
            obj->SetPool(this);
            _pool.push_back(obj);
        }
    }

    ~ObjectPool()
    {
        for (auto *obj : _pool)
        {
            delete obj;
        }
    }

    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

    template<typename... Args>
    std::shared_ptr<T> Acquire(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_pool.empty())
        {
            for (std::size_t i = 0; i < _grow_size; ++i)
            {
                auto *obj = new T();
                obj->SetPool(this);
                _pool.push_back(obj);
            }
        }

        T *obj = _pool.back();
        _pool.pop_back();

        if constexpr (sizeof...(Args) == 0)
        {
            obj->Reset();
        }
        else
        {
            obj->Init(std::forward<Args>(args)...);
        }

        return std::shared_ptr<T>(obj, [this](T *ptr)
        {
            ptr->Reset();
            std::lock_guard<std::mutex> lock(_mutex);
            _pool.push_back(ptr);
        });
    }

private:
    std::mutex _mutex;
    std::size_t _grow_size;
    std::vector<T *> _pool;
};

#endif

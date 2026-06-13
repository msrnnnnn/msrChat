#pragma once
/**
 * @file ObjectPool.h
 * @brief 通用对象池模板 —— 预分配对象并循环复用，避免频繁 new/delete
 * @details 池中对象必须提供 SetPool()、Reset()、Init() 方法。
 *          Acquire() 返回 shared_ptr，自定义删除器将对象归还池中。
 *          池耗尽时自动按 grow_size 扩容。
 */
#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

/**
 * @brief 线程安全的对象池
 * @tparam T 池中对象类型，要求实现 SetPool()、Reset()、Init() 接口
 */
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
        _destroying.store(true, std::memory_order_release);
        for (auto *obj : _pool)
        {
            delete obj;
        }
    }

    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

    /**
     * @brief 从池中获取一个对象（shared_ptr，归还时自动调用 Reset() 并 push 回池）
     * @param args 可选初始化参数，有参数时调用 obj->Init(args...)，无参数时调用 obj->Reset()
     * @return shared_ptr 持有对象，引用计数归零时对象自动归还池中
     */
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
            if (_destroying.load(std::memory_order_acquire))
            {
                delete ptr;
                return;
            }
            std::lock_guard<std::mutex> lock(_mutex);
            _pool.push_back(ptr);
        });
    }

private:
    std::mutex _mutex;
    std::size_t _grow_size;
    std::vector<T *> _pool;
    std::atomic<bool> _destroying{false};
};

#endif

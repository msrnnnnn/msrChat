#pragma once
/**
 * @file singleton.h
 * @brief 单例模板基类 - 强制手动生命周期管理
 * @details 使用 CRTP (Curiously Recurring Template Pattern) 实现。
 *          必须显式调用 Init() 和 Destroy() 管理生命周期，避免静态对象析构顺序问题。
 */
#ifndef SINGLETON_H
#define SINGLETON_H

#include <atomic>
#include <mutex>

template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    virtual ~Singleton() = default;

public:
    static T* Instance() {
        return _instance.load(std::memory_order_acquire);
    }

    static void Init() {
        T* tmp = _instance.load(std::memory_order_acquire);
        if (tmp) {
            return;
        }
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        tmp = _instance.load(std::memory_order_relaxed);
        if (!tmp) {
            tmp = new T();
            _instance.store(tmp, std::memory_order_release);
        }
    }

    static void Destroy() {
        T* tmp = _instance.exchange(nullptr, std::memory_order_acq_rel);
        delete tmp;
    }

private:
    static std::atomic<T*> _instance;
};

template <typename T>
std::atomic<T*> Singleton<T>::_instance{nullptr};

#endif

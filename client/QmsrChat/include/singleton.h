/**
 * @file singleton.h
 * @brief 单例模板基类 - 强制手动生命周期管理
 * @details 使用 CRTP (Curiously Recurring Template Pattern) 实现。
 *          必须显式调用 Init() 和 Destroy() 管理生命周期，避免静态对象析构顺序问题。
 */
#ifndef SINGLETON_H
#define SINGLETON_H

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
        return _instance;
    }

    static void Init() {
        if (!_instance) {
            static std::mutex mtx;
            std::lock_guard<std::mutex> lock(mtx);
            if (!_instance) {
                _instance = new T();
            }
        }
    }

    static void Destroy() {
        if (_instance) {
            delete _instance;
            _instance = nullptr;
        }
    }

private:
    static T* _instance;
};

template <typename T>
T* Singleton<T>::_instance = nullptr;

#endif

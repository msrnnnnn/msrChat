/**
 * @file singleton.h
 * @brief 线程安全的单例模板基类
 * @details 使用 CRTP (Curiously Recurring Template Pattern) 实现。
 *          利用 C++11 静态局部变量特性保证初始化时的线程安全。
 */
#ifndef SINGLETON_H
#define SINGLETON_H
#include <memory>
#include <mutex>
#include <iostream>

/**
 * @class Singleton
 * @brief 线程安全的单例模板基类
 * @tparam T 需要实现单例的具体类
 */
template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    virtual ~Singleton() {
        // 可在此处添加析构日志
    }

public:
    /**
     * @brief 获取单例实例
     * @return std::shared_ptr<T> 指向单例的智能指针
     */
    static std::shared_ptr<T> GetInstance() {
        // C++11 保证静态局部变量初始化的线程安全性
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }

    /**
     * @brief 获取单例原始指针
     * @return T* 原始指针
     */
    static T* getPtr() {
        return GetInstance().get();
    }
};

#endif // SINGLETON_H

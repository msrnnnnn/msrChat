/**
 * @file    Singleton.h
 * @brief   线程安全的单例模板基类
 * @details 基于 CRTP 实现。
 *          利用 C++11 Static Local Variable 的特性保证线程安全。
 * @author  msr
 */

#pragma once

#include <iostream>
#include <memory>
#include <mutex>

/**
 * @class   Singleton
 * @brief   单例模板类
 * @tparam  T 需要实现单例的具体类
 */
template <typename T>
class Singleton
{
public:
    /**
     * @brief 获取单例实例 (Lazy Initialization)
     * @return std::shared_ptr<T> 单例的共享指针
     */
    static std::shared_ptr<T> GetInstance()
    {
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }

    /**
     * @brief 获取原生指针
     * @return T* 原始指针
     */
    static T *getPtr()
    {
        return GetInstance().get();
    }

    /**
     * @brief 打印析构日志
     */
    void PrintDestructor()
    {
        std::cout << "this is singleton destruct" << std::endl;
    }

protected:
    /**
     * @brief 默认构造函数
     */
    Singleton() = default;

    /**
     * @brief 禁止拷贝构造
     */
    Singleton(const Singleton &) = delete;

    /**
     * @brief 禁止赋值操作
     */
    Singleton &operator=(const Singleton &) = delete;

    /**
     * @brief 虚析构函数
     */
    virtual ~Singleton()
    {
        std::cout << " this is ~Singleton() " << std::endl;
    }
};

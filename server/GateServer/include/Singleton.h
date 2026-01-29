<<<<<<< HEAD
/**
 * @file Singleton.h
 * @brief 线程安全的单例模板基类 (Thread-Safe Singleton Base)
 * @details
 * 基于 CRTP (Curiously Recurring Template Pattern) 实现。
 * 利用 C++11 Static Local Variable 的特性保证线程安全，无需加锁。
 * @author C++ Architect
 */

#pragma once
#include <iostream>
#include <memory>
#include <mutex>

/**
 * @brief 单例模板类
 * @tparam T 需要实现单例的具体类
 * @note [Design Pattern]
 * 继承此类后，T 将自动获得单例属性。
 * T 的构造函数应设为 private，并声明 `friend class Singleton<T>;` 以允许基类创建实例。
 */
template <typename T>
class Singleton
{
public:
    /**
     * @brief 获取单例实例 (Lazy Initialization)
     * @return std::shared_ptr<T> 单例的共享指针
     * * @note [Thread Safety] (C++11 Magic Static)
     * 在 C++11 及以后，静态局部变量的初始化是线程安全的。
     * 编译器会由链接器 (Linker) 和运行时 (Runtime) 保证：
     * 如果多个线程同时尝试初始化 _instance，只有一个会成功，其他线程会阻塞直到初始化完成。
     * 这完全替代了传统的 "Double-Checked Locking" (双重检查锁) 模式。
     */
    static std::shared_ptr<T> GetInstance()
    {
        /**
         * @warning [Memory Allocation]
         * 这里使用 `new T` 而不是 `std::make_shared<T>`。
         * 原因：T 的构造函数通常是 private 的 (为了强制单例)。
         * `make_shared` 无法访问 private 构造函数，除非侵入式地修改 T 的代码 (friend std::allocator)。
         * 直接 `new T` 只需要 T friend Singleton 即可，侵入性较小。
         */
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }

    /**
     * @brief 获取原生指针 (Unsafe Access)
     * @return T* 原始指针
     * @warning 仅用于遗留代码兼容，不建议直接使用。
     */
    static T *getPtr()
    {
        return GetInstance().get();
    }

    /**
     * @brief 打印析构日志
     * @details 仅用于调试，生产环境建议移除或使用标准日志库
     */
    void PrintDestructor()
    {
        std::cout << "this is singleton destruct" << std::endl;
    }

protected:
    // @brief 允许派生类构造，但禁止外部直接构造
    Singleton() = default;

    // @brief 禁止拷贝构造 (Non-Copyable)
    Singleton(const Singleton &) = delete;

    // @brief 禁止赋值操作 (Non-Assignable)
    Singleton &operator=(const Singleton &) = delete;

    /**
     * @brief 虚析构函数
     * @details
     * 虽然单例通常随程序结束而销毁，但为了防止
     * `Singleton<T>* p = new T(); delete p;` 导致的未定义行为，
     * 基类析构函数应为 virtual。
     */
    virtual ~Singleton()
    {
        std::cout << " this is ~Singleton() " << std::endl;
    }
=======
/**
 * @file Singleton.h
 * @brief 线程安全的单例模板基类 (Thread-Safe Singleton Base)
 * @details
 * 基于 CRTP (Curiously Recurring Template Pattern) 实现。
 * 利用 C++11 Static Local Variable 的特性保证线程安全，无需加锁。
 * @author C++ Architect
 */

#pragma once
#include <iostream>
#include <memory>
#include <mutex>

/**
 * @brief 单例模板类
 * @tparam T 需要实现单例的具体类
 * @note [Design Pattern]
 * 继承此类后，T 将自动获得单例属性。
 * T 的构造函数应设为 private，并声明 `friend class Singleton<T>;` 以允许基类创建实例。
 */
template <typename T>
class Singleton
{
public:
    /**
     * @brief 获取单例实例 (Lazy Initialization)
     * @return std::shared_ptr<T> 单例的共享指针
     * * @note [Thread Safety] (C++11 Magic Static)
     * 在 C++11 及以后，静态局部变量的初始化是线程安全的。
     * 编译器会由链接器 (Linker) 和运行时 (Runtime) 保证：
     * 如果多个线程同时尝试初始化 _instance，只有一个会成功，其他线程会阻塞直到初始化完成。
     * 这完全替代了传统的 "Double-Checked Locking" (双重检查锁) 模式。
     */
    static std::shared_ptr<T> GetInstance()
    {
        /**
         * @warning [Memory Allocation]
         * 这里使用 `new T` 而不是 `std::make_shared<T>`。
         * 原因：T 的构造函数通常是 private 的 (为了强制单例)。
         * `make_shared` 无法访问 private 构造函数，除非侵入式地修改 T 的代码 (friend std::allocator)。
         * 直接 `new T` 只需要 T friend Singleton 即可，侵入性较小。
         */
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }

    /**
     * @brief 获取原生指针 (Unsafe Access)
     * @return T* 原始指针
     * @warning 仅用于遗留代码兼容，不建议直接使用。
     */
    static T *getPtr()
    {
        return GetInstance().get();
    }

    /**
     * @brief 打印析构日志
     * @details 仅用于调试，生产环境建议移除或使用标准日志库
     */
    void PrintDestructor()
    {
        std::cout << "this is singleton destruct" << std::endl;
    }

protected:
    // @brief 允许派生类构造，但禁止外部直接构造
    Singleton() = default;

    // @brief 禁止拷贝构造 (Non-Copyable)
    Singleton(const Singleton &) = delete;

    // @brief 禁止赋值操作 (Non-Assignable)
    Singleton &operator=(const Singleton &) = delete;

    /**
     * @brief 虚析构函数
     * @details
     * 虽然单例通常随程序结束而销毁，但为了防止
     * `Singleton<T>* p = new T(); delete p;` 导致的未定义行为，
     * 基类析构函数应为 virtual。
     */
    virtual ~Singleton()
    {
        std::cout << " this is ~Singleton() " << std::endl;
    }
>>>>>>> d4bdb55d943f6b6232f392fe15188f9e5cb730f4
};
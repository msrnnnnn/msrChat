/**
 * @file Singleton.h
 * @brief Meyer's Singleton 模式实现
 * @details 线程安全的单例模板，使用局部静态变量实现
 */

#ifndef SINGLETON_H
#define SINGLETON_H

/**
 * @class Singleton
 * @brief 单例模板类
 * @tparam T 需要实现单例的类
 * 
 * 使用 Meyer's Singleton 模式，在 C++11 及以后的标准中天然线程安全
 * 延迟初始化：第一次调用 getInstance() 时才创建实例
 */
template<typename T>
class Singleton {
public:
    /**
     * @brief 获取单例实例
     * @return T& 单例实例的引用
     */
    static T& getInstance() {
        static T instance;
        return instance;
    }

    // 禁止拷贝构造
    Singleton(const Singleton&) = delete;
    
    // 禁止赋值操作
    Singleton& operator=(const Singleton&) = delete;

protected:
    // 保护构造函数，防止外部实例化
    Singleton() = default;
    
    // 保护析构函数
    virtual ~Singleton() = default;
};

#endif // SINGLETON_H

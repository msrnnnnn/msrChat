#ifndef SINGLETON_H
#define SINGLETON_H
#include "global.h"

/**
 * 这段代码定义了一个“单例生成器”。它的作用是：任何类只要继承了这个模板类，就自动拥有了单例属性，无需重复编写 GetInstance 等样板代码。
 */

template <typename T>
class Singleton{
protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    virtual ~Singleton(){
        std::cout << "this is ~Singleton() " << std::endl;
    }
public:
    static std::shared_ptr<T> GetInstance(){
        static std::shared_ptr<T> _instance(new T);//在 C++11 及以后标准中，局部静态变量的初始化是线程安全的，因此不需要额外的 std::mutex 加锁
        return _instance;
    }
    static T* getPtr(){
        return GetInstance().get();
    }

};

#endif // SINGLETON_H

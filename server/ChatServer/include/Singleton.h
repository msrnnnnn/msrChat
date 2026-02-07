#pragma once
#include <iostream>
#include <memory>
#include <mutex>

template <typename T>
class Singleton
{
public:
    static std::shared_ptr<T> GetInstance()
    {
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }

    static T *getPtr()
    {
        return GetInstance().get();
    }

    void PrintDestructor()
    {
        std::cout << "this is singleton destruct" << std::endl;
    }

protected:
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
    virtual ~Singleton()
    {
        std::cout << " this is ~Singleton() " << std::endl;
    }
};

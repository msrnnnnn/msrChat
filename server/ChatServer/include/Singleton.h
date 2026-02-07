#pragma once
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>

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
        spdlog::info("this is singleton destruct");
    }

protected:
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
    virtual ~Singleton()
    {
        spdlog::info(" this is ~Singleton() ");
    }
};

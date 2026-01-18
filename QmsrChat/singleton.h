#ifndef SINGLETON_H
#define SINGLETON_H
#include "global.h"

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
        static std::shared_ptr<T> _instance(new T);
        return _instance;
    }
    static T* getPtr(){
        return GetInstance().get();
    }

};

#endif // SINGLETON_H

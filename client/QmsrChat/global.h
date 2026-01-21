#ifndef GLOBAL_H
#define GLOBAL_H

#include <QWidget>
#include <functional>
#include <iostream>
#include <mutex>
#include <memory>


/**
 * @brief repolish 用来刷新qss
 */
extern std::function<void(QWidget*)> repolish;

enum class RequestType{
    ID_GET_VARIFY_CODE = 1001,      //获取验证码
    ID_REGISTER_USER = 1002,        //注册用户
};

enum class Modules{
    REGISTER_MOD = 0, //注册模块
    LOGIN_MOD = 1,    //登录模块
};

enum class ERRORCODES{
    SUCCESS = 0,
    ERROR_JOSN = 1,     //josn解析失败
    ERROR_NETWORK = 2,  //网络错误
};

#endif // GLOBAL_H

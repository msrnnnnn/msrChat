/**
 * @file global.h
 * @brief 全局定义头文件 (Global Definitions)
 * @details 包含全局变量声明、枚举类型定义及通用工具函数。
 * @author msr
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>
#include <QWidget>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

/**
 * @brief 网关 URL 前缀配置
 * @details 用于存储连接网关服务器的 URL 前缀。
 */
extern QString gate_url_prefix;

extern std::function<QString(QString)> xorString;

/**
 * @brief 刷新 QSS 样式函数对象
 * @param w 需要刷新样式的 QWidget 指针
 * @details 调用 QStyle 的 unpolish 和 polish 方法强制刷新控件样式。
 */
extern std::function<void(QWidget *)> repolish;

/**
 * @brief 请求类型枚举
 * @details 定义客户端发送给服务器的请求类型 ID。
 */
enum class RequestType
{
    ID_GET_VARIFY_CODE = 1001, ///< 获取验证码请求
    ID_REGISTER_USER = 1002,    ///< 注册用户请求
    ID_RESET_PWD = 1003,       ///< 重置密码
    ID_LOGIN_USER = 1004,      ///< 用户登录
};

struct ServerInfo {
    QString Host;
    QString Port;
    QString Token;
    int Uid;
};

/**
 * @brief 功能模块枚举
 * @details 标识应用程序的不同功能模块。
 */
enum class Modules
{
    REGISTER_MOD = 0, ///< 注册模块
    RESETMOD = 1,     ///< 重置密码模块
    LOGINMOD = 2,     ///< 登录模块
};

/**
 * @brief 错误码枚举
 * @details 定义应用程序通用的错误代码。
 */
enum class ERRORCODES
{
    SUCCESS = 0,      ///< 操作成功
    ERROR_JSON = 1,   ///< JSON 解析失败
    ERROR_NETWORK = 2, ///< 网络通信错误
    // 业务逻辑错误码 (1000+)
    UserExist = 1001,       ///< 用户名已存在
    PasswdErr = 1002,       ///< 密码错误
    EmailExist = 1003,      ///< 邮箱已存在
    VarifyCodeErr = 1004,   ///< 验证码错误
    VarifyCodeExpired = 1005 ///< 验证码过期
};

#endif // GLOBAL_H

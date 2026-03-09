/**
 * @file global.h
 * @brief 全局定义头文件
 * @details 包含全局变量声明、枚举类型定义及通用工具函数。
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
 * @brief 网关服务器 URL 前缀
 */
extern QString gate_url_prefix;

/**
 * @brief 异或字符串处理函数
 * @details 用于简单的敏感数据混淆。
 */
extern std::function<QString(QString)> xorString;

/**
 * @brief 刷新控件样式
 * @param w 需要刷新样式的 QWidget 指针
 */
extern std::function<void(QWidget *)> repolish;

/**
 * @brief 网络请求类型枚举
 */
enum class RequestType
{
    ID_GET_VARIFY_CODE = 1001, ///< 获取验证码
    ID_REGISTER_USER = 1002,   ///< 用户注册
    ID_RESET_PWD = 1003,       ///< 重置密码
    ID_LOGIN_USER = 1004,      ///< 用户登录
    ID_CHAT_LOGIN = 1005,      ///< 聊天服务登录
};

/**
 * @brief 服务器连接信息结构体
 */
struct ServerInfo {
    QString Host;   ///< 主机地址
    QString Port;   ///< 端口号
    QString Token;  ///< 认证令牌
    int Uid;        ///< 用户 ID
};

/**
 * @brief 功能模块标识枚举
 */
enum class Modules
{
    REGISTER_MOD = 0, ///< 注册模块
    RESETMOD = 1,     ///< 重置密码模块
    LOGINMOD = 2,     ///< 登录模块
};

/**
 * @brief 全局错误码定义
 */
enum class ERRORCODES
{
    SUCCESS = 0,            ///< 操作成功
    ERROR_JSON = 1001,      ///< JSON 解析失败
    RPC_FAILED = 1002,      ///< RPC 调用失败
    VarifyCodeExpired = 1003, ///< 验证码已过期
    VarifyCodeErr = 1004,     ///< 验证码错误
    UserExist = 1005,         ///< 用户名已存在
    PasswdErr = 1006,         ///< 密码错误
    UserNotExist = 1007,      ///< 用户不存在
    EmailNotMatch = 1008,     ///< 邮箱不匹配
    PasswdUpFailed = 1009,    ///< 密码更新失败
    RPCGetFailed = 1010,      ///< 获取状态服务失败
    ERROR_NETWORK = 2         ///< 网络通信错误
};

/**
 * @brief 输入校验错误类型
 */
enum class TipErr
{
    TIP_SUCCESS = 0,
    TIP_EMAIL_ERR = 1,
    TIP_PWD_ERR = 2,
    TIP_CONFIRM_ERR = 3,
    TIP_PWD_CONFIRM = 4,
    TIP_VARIFY_ERR = 5,
    TIP_USER_ERR = 6
};

/**
 * @brief 点击标签状态枚举
 */
enum class ClickLbState
{
    Normal = 0,   ///< 正常状态
    Selected = 1  ///< 选中状态
};

#endif // GLOBAL_H

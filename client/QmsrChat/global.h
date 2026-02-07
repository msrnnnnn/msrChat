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

/**
 * @brief 刷新 QSS 样式函数对象
 * @param w 需要刷新样式的 QWidget 指针
 * @details 调用 QStyle 的 unpolish 和 polish 方法强制刷新控件样式。
 */
extern std::function<void(QWidget *)> repolish;

/**
 * @brief 简单的服务器信息结构体
 */
struct ServerInfo
{
    QString Uid;
    QString Host;
    QString Port;
    QString Token;
};

/**
 * @brief 请求ID类型定义 (枚举类)
 * @details 定义客户端发送给服务器的请求类型 ID。
 */
enum class ReqId
{
    ID_GET_VARIFY_CODE = 1001, ///< 获取验证码请求
    ID_REGISTER_USER = 1002,   ///< 注册用户请求
    ID_RESET_PWD = 1003,       ///< 重置密码请求
    ID_USER_LOGIN = 1004,      ///< 用户登录请求
    ID_CHAT_LOGIN = 1005,      ///< 聊天登录请求
    ID_CHAT_LOGIN_RSP = 1006   ///< 聊天登录回复
};

/**
 * @brief 功能模块枚举
 * @details 标识应用程序的不同功能模块。
 */
enum class Modules
{
    REGISTER_MOD = 0, ///< 注册模块
    LOGIN_MOD = 1,    ///< 登录模块
    RESET_MOD = 2     ///< 重置密码模块
};

/**
 * @brief 错误码枚举
 * @details 定义应用程序通用的错误代码。
 */
enum class ERRORCODES
{
    SUCCESS = 0,      ///< 操作成功
    ERROR_JSON = 1,   ///< JSON 解析失败
    ERROR_NETWORK = 2 ///< 网络通信错误
};

// 兼容别名
using ErrorCodes = ERRORCODES;
using RequestType = ReqId; // 暂时兼容，逐步替换

/**
 * @brief 输入校验错误类型枚举
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
    Normal = 0,  ///< 普通状态
    Selected = 1 ///< 选中状态
};

#endif // GLOBAL_H

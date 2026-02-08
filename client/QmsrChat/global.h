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
    ID_GET_VARIFY_CODE = 1001,          ///< 获取验证码请求
    ID_REGISTER_USER = 1002,            ///< 注册用户请求
    ID_RESET_PWD = 1003,                ///< 重置密码请求
    ID_USER_LOGIN = 1004,               ///< 用户登录请求
    ID_CHAT_LOGIN = 1005,               ///< 聊天登录请求
    ID_CHAT_LOGIN_RSP = 1006,           ///< 聊天登录回复
    ID_SEARCH_USER_REQ = 1007,          ///< 搜索用户请求
    ID_SEARCH_USER_RSP = 1008,          ///< 搜索用户回复
    ID_ADD_FRIEND_REQ = 1009,           ///< 申请添加好友请求
    ID_ADD_FRIEND_RSP = 1010,           ///< 申请添加好友回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,    ///< 通知用户有好友申请
    ID_AUTH_FRIEND_REQ = 1012,          ///< 认证好友请求
    ID_AUTH_FRIEND_RSP = 1013,          ///< 认证好友回复
    ID_NOTIFY_AUTH_FRIEND_REQ = 1014,   ///< 通知用户好友认证通过
    ID_TEXT_CHAT_MSG_REQ = 1015,        ///< 文本聊天消息请求
    ID_TEXT_CHAT_MSG_RSP = 1016,        ///< 文本聊天消息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1017, ///< 通知用户有文本消息
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
    SUCCESS = 0,
    ERR_JSON = 1,           ///< JSON 解析失败
    ERR_NETWORK = 2,        ///< 网络通信错误
    VARIFY_EXPIRED = 1001,  ///< 验证码过期
    VARIFY_CODE_ERR = 1002, ///< 验证码错误
    EMAIL_NOT_MATCH = 1003, ///< 邮箱不匹配
    PASSWD_UP_FAILED = 1004 ///< 密码更新失败
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

/**
 * @brief 列表项类型枚举
 */
enum class ListItemType
{
    CHAT_USER_ITEM = 0,    ///< 聊天列表项
    CONTACT_USER_ITEM = 1, ///< 联系人列表项
    SEARCH_USER_ITEM = 2,  ///< 搜索用户列表项
    APPLY_FRIEND_ITEM = 3, ///< 好友申请列表项
};

/**
 * @brief 聊天角色枚举
 */
enum class ChatRole
{
    Self, ///< 自己
    Other ///< 别人
};

/**
 * @brief 消息信息结构体
 */
struct MsgInfo
{
    QString msgId;
    QString fromUid;
    QString toUid;
    QString content;
    QString msgFlag; // "text", "image", "file"
    QString time;
    int status;
};

/**
 * @brief 搜索用户信息结构体
 */
struct SearchInfo
{
    int uid;
    QString name;
    QString nick;
    QString desc;
    int sex;
    QString icon;
};

#endif // GLOBAL_H

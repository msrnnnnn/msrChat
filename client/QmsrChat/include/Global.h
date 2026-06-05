/**
 * @file global.h
 * @brief 全局定义头文件
 * @details 包含全局变量声明、枚举类型定义及通用工具函数。
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#include <QMetaType>
#include <QString>

/**
 * @brief 网络请求类型枚举
 */
enum class RequestType
{
    MSG_HELLO = 1000,
    ID_GET_VARIFY_CODE = 1001, ///< 获取验证码
    ID_REGISTER_USER = 1002,   ///< 用户注册
    ID_RESET_PWD = 1003,       ///< 重置密码
    ID_LOGIN_USER = 1004,      ///< 用户登录
    MSG_CHAT_LOGIN = 1005,
    MSG_CHAT_TEXT = 1006,
    MSG_CHAT_ACK = 1007,
    MSG_OFFLINE_ACK = 1008,       ///< 离线消息分页确认
    MSG_CHAT_IMAGE = 1009,          ///< 图片消息
    MSG_IMAGE_DOWNLOAD_RSP = 1010,  ///< 图片下载响应
    MSG_CHAT_RECALL = 1011,        ///< 消息撤回
    MSG_CHAT_EDIT = 1012,           ///< 消息编辑
    MSG_IMAGE_DOWNLOAD_REQ = 1013, ///< 图片下载请求
    MSG_CHAT_RECALL_NOTIFY = 1014, ///< 撤回通知
    MSG_CHAT_EDIT_NOTIFY = 1015,   ///< 编辑通知
    MSG_FILE_REQ = 2001,          ///< 文件传输请求
    MSG_FILE_RSP = 2002,          ///< 文件传输响应(断点续传)
    MSG_FILE_CHUNK = 2003,        ///< 文件数据分片
    MSG_FILE_ACK = 2004,          ///< 数据块接收确认
};

Q_DECLARE_METATYPE(RequestType)

/**
 * @brief 服务器连接信息结构体
 */
struct ServerInfo
{
    QString Host;  ///< 主机地址
    QString Port;  ///< 端口号
};

Q_DECLARE_METATYPE(ServerInfo)

/**
 * @brief 全局错误码定义
 */
enum class ERRORCODES
{
    SUCCESS = 0,              ///< 操作成功
    VarifyCodeExpired = 1003, ///< 验证码已过期
    VarifyCodeErr = 1004,     ///< 验证码错误
    UserExist = 1005,         ///< 用户名已存在
    PasswdErr = 1006,         ///< 密码错误
    UserNotExist = 1007,      ///< 用户不存在
    EmailNotMatch = 1008,     ///< 邮箱不匹配
    PasswdUpFailed = 1009,    ///< 密码更新失败
    RPCGetFailed = 1010,      ///< 获取状态服务失败
};

/**
 * @brief 输入校验错误类型
 */
enum class TipErr
{
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
    Normal = 0,  ///< 正常状态
    Selected = 1 ///< 选中状态
};

#endif // GLOBAL_H

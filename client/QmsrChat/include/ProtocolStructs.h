#pragma once
/**
 * @file ProtocolStructs.h
 * @brief 网络协议强类型数据结构体
 * @details 定义所有网络回包的强类型结构，消除UI层对JSON的直接依赖
 */
#ifndef PROTOCOLSTRUCTS_H
#define PROTOCOLSTRUCTS_H

#include <QMetaType>
#include <QString>
#include <QtGlobal>

/**
 * @brief 登录请求
 */
struct LoginReqStruct
{
    QString user;   ///< 用户名
    QString passwd; ///< 密码
};

/**
 * @brief 聊天登录请求
 */
struct ChatLoginReqStruct
{
    int uid;        ///< 用户ID
    QString token;  ///< 登录令牌
};

/**
 * @brief 聊天文本消息请求
 */
struct ChatTextReqStruct
{
    int from_uid;           ///< 发送方UID
    int to_uid;             ///< 接收方UID
    QString content;        ///< 消息内容
    QString client_msg_id;  ///< 客户端消息ID（去重）
    qint64 timestamp;       ///< 客户端毫秒时间戳
};

/**
 * @brief 登录响应
 */
struct LoginRspStruct
{
    int error;        ///< 错误码（0成功）
    int uid;          ///< 用户ID
    QString token;    ///< 登录令牌
    QString user;     ///< 用户名
    QString message;  ///< 错误信息
};

/**
 * @brief 聊天登录响应
 */
struct ChatLoginRspStruct
{
    int error;         ///< 错误码（0成功）
    QString message;   ///< 响应消息
};

/**
 * @brief 聊天文本消息
 */
struct ChatTextMsgStruct
{
    int from_uid;           ///< 发送方UID
    int to_uid;             ///< 接收方UID
    QString content;        ///< 消息内容
    QString client_msg_id;  ///< 客户端消息ID
    qint64 server_msg_id;   ///< 服务端消息ID（全局唯一）
    qint64 timestamp;       ///< 消息时间戳
};

/**
 * @brief 消息确认（ACK）
 */
struct ChatAckStruct
{
    int error;               ///< 错误码
    QString message;         ///< 确认消息
    QString client_msg_id;   ///< 对应的客户端消息ID
};

/**
 * @brief 离线消息分页确认
 */
struct OfflineAckStruct
{
    qint64 received;  ///< 已接收数
    qint64 total;     ///< 总数
};

/**
 * @brief 离线消息分页确认请求
 */
struct OfflineAckReqStruct
{
    qint64 received;  ///< 已接收数
};

/**
 * @brief 验证码请求
 */
struct VerifyCodeReqStruct
{
    QString email;  ///< 邮箱地址
};

/**
 * @brief 验证码响应
 */
struct VerifyCodeRspStruct
{
    int error;        ///< 错误码
    QString email;    ///< 邮箱地址
    int code = 0;     ///< 验证码
    QString message;  ///< 错误信息
};

/**
 * @brief 注册请求
 */
struct RegisterReqStruct
{
    QString user;        ///< 用户名
    QString email;       ///< 邮箱
    QString passwd;      ///< 密码
    QString verifycode;  ///< 验证码
};

/**
 * @brief 注册响应
 */
struct RegisterRspStruct
{
    int error;       ///< 错误码
    QString email;   ///< 注册邮箱
    QString message; ///< 错误信息（服务端返回的详细描述）
};

/**
 * @brief 重置密码请求
 */
struct ResetPwdReqStruct
{
    QString user;        ///< 用户名
    QString email;       ///< 邮箱
    QString passwd;      ///< 新密码
    QString verifycode;  ///< 验证码
};

/**
 * @brief 重置密码响应
 */
struct ResetPwdRspStruct
{
    int error;        ///< 错误码
    QString message;  ///< 错误信息
};

Q_DECLARE_METATYPE(ChatTextMsgStruct)
Q_DECLARE_METATYPE(ChatAckStruct)
Q_DECLARE_METATYPE(OfflineAckStruct)
Q_DECLARE_METATYPE(LoginRspStruct)
Q_DECLARE_METATYPE(ChatLoginRspStruct)

/**
 * @brief 文件传输请求（含断点续传偏移量）
 */
struct FileReqStruct
{
    int64_t task_id;     ///< 任务ID
    int from_uid;        ///< 发送方UID
    int to_uid;          ///< 接收方UID
    QString filename;    ///< 文件名
    int64_t total_size;  ///< 文件总大小（字节）
    QString md5;         ///< 文件MD5值
    int64_t offset = 0;  ///< 续传偏移量（0表示新传输）
};

/**
 * @brief 图片消息结构体
 */
struct ChatImageStruct
{
    int from_uid = 0;        ///< 发送方UID
    int to_uid = 0;          ///< 接收方UID
    QString image_id;        ///< 图片唯一标识
    QString caption;         ///< 图片说明文字
    qint64 timestamp = 0;    ///< 时间戳
    int width = 0;           ///< 图片宽度
    int height = 0;          ///< 图片高度
    QString ext;             ///< 文件扩展名
    qint64 size = 0;         ///< 文件大小（字节）
    QString md5;             ///< 文件MD5值
};

/**
 * @brief 图片下载响应
 */
struct ImageDownloadRspStruct
{
    int error = 0;           ///< 错误码（0成功）
    QString image_id;        ///< 图片ID
    qint64 offset = 0;       ///< 已接收的字节偏移量
};

/**
 * @brief 撤回消息请求
 */
struct ChatRecallMsgStruct
{
    int from_uid = 0;             ///< 发起撤回的UID
    qint64 msg_timestamp = 0;     ///< 被撤回消息的时间戳
    QString client_msg_id;        ///< 被撤回消息的客户端ID
};

/**
 * @brief 撤回通知（广播给聊天双方）
 */
struct ChatRecallNotifyStruct
{
    qint64 msg_timestamp = 0;  ///< 被撤回消息的时间戳
    int recall_uid = 0;        ///< 发起撤回的UID
    int recalled_to = 0;       ///< 撤回目标UID
    qint64 recall_ts = 0;      ///< 撤回操作的时间戳
};

/**
 * @brief 编辑消息请求
 */
struct ChatEditMsgStruct
{
    int from_uid = 0;              ///< 发送方UID
    qint64 msg_timestamp = 0;      ///< 被编辑消息的时间戳
    QString new_content;           ///< 编辑后的新内容
};

/**
 * @brief 编辑消息确认
 */
struct ChatEditAckStruct
{
    int error = 0;                 ///< 错误码
    QString message;               ///< 响应消息
    qint64 msg_timestamp = 0;      ///< 被编辑消息的时间戳
    qint64 edit_ts = 0;            ///< 编辑操作的时间戳
    QString new_content;           ///< 编辑成功后服务端回传的新内容
};

/**
 * @brief 编辑通知（广播给聊天双方）
 */
struct ChatEditNotifyStruct
{
    qint64 msg_timestamp = 0;  ///< 被编辑消息的时间戳
    int from_uid = 0;          ///< 编辑者UID
    QString new_content;       ///< 编辑后的新内容
    qint64 edit_ts = 0;        ///< 编辑操作的时间戳
};

Q_DECLARE_METATYPE(ChatImageStruct)
Q_DECLARE_METATYPE(ImageDownloadRspStruct)
Q_DECLARE_METATYPE(ChatRecallMsgStruct)
Q_DECLARE_METATYPE(ChatRecallNotifyStruct)
Q_DECLARE_METATYPE(ChatEditMsgStruct)
Q_DECLARE_METATYPE(ChatEditAckStruct)
Q_DECLARE_METATYPE(ChatEditNotifyStruct)

#endif // PROTOCOLSTRUCTS_H

#pragma once
/**
 * @file const.h
 * @brief ChatServer 协议与配置常量
 */
#ifndef CONST_H
#define CONST_H

#include <chrono>
#include <cstdint>

constexpr uint16_t MSG_HELLO = 1000;             ///< 心跳消息 (0x3E8)
constexpr uint16_t ID_GET_VERIFY_CODE = 1001;    ///< 获取验证码
constexpr uint16_t ID_REGISTER_USER = 1002;      ///< 用户注册
constexpr uint16_t ID_RESET_PWD = 1003;          ///< 重置密码
constexpr uint16_t ID_LOGIN_USER = 1004;         ///< 用户登录
constexpr uint16_t MSG_CHAT_LOGIN = 1005;        ///< 聊天会话登录
constexpr uint16_t MSG_CHAT_TEXT = 1006;         ///< 聊天文本消息
constexpr uint16_t MSG_CHAT_ACK = 1007;          ///< 聊天消息确认
constexpr uint16_t MSG_OFFLINE_ACK = 1008;       ///< 离线消息分页确认
constexpr uint16_t MSG_CHAT_IMAGE         = 1009;  ///< 图片消息
constexpr uint16_t MSG_IMAGE_DOWNLOAD_RSP = 1010;  ///< 图片下载响应 (server→client) — 新增
constexpr uint16_t MSG_CHAT_RECALL        = 1011;  ///< 消息撤回
constexpr uint16_t MSG_CHAT_EDIT          = 1012;  ///< 消息编辑
constexpr uint16_t MSG_IMAGE_DOWNLOAD_REQ = 1013;  ///< 离线图片下载请求（顺延）
constexpr uint16_t MSG_CHAT_RECALL_NOTIFY = 1014;  ///< 撤回通知（server→client）
constexpr uint16_t MSG_CHAT_EDIT_NOTIFY   = 1015;  ///< 编辑通知（server→client）
constexpr uint16_t MSG_FILE_REQ = 2001;          ///< 文件传输请求
constexpr uint16_t MSG_FILE_RSP = 2002;          ///< 文件传输响应(断点续传)
constexpr uint16_t MSG_FILE_CHUNK = 2003;        ///< 文件数据分片(Protobuf 消息体)
constexpr uint16_t MSG_FILE_ACK = 2004;          ///< 数据块接收确认(Protobuf 消息体)
constexpr int MAX_CHAT_CONTENT_LEN = 4096;        ///< 单条消息最大长度
constexpr int OFFLINE_PAGE_SIZE = 50;            ///< 离线消息每页数量

// 协议头部常量
constexpr int HEAD_ID_LEN = 2;          ///< 消息 ID 字节长度
constexpr int HEAD_DATA_LEN = 4;        ///< 消息体长度字段字节数
constexpr int HEAD_TOTAL_LEN = 6;       ///< 头部总长度（仅包含ID和总长度）
constexpr int MAX_LENGTH = 1024 * 1024; ///< 单包最大长度

// 读取超时
constexpr auto kReadTimeout = std::chrono::seconds(30);
constexpr auto kReadCheckInterval = std::chrono::seconds(5);

// 文件传输
constexpr size_t CHUNK_SIZE = 4 * 1024;

// Phase 4 — 消息体大小分级限制
constexpr size_t MAX_TEXT_MSG_SIZE    = 64 * 1024;    ///< 文本消息 ≤64KB
constexpr size_t MAX_FILE_META_SIZE   = 4 * 1024;     ///< 文件元数据 ≤4KB
constexpr size_t MAX_CHUNK_SIZE       = 1024 * 1024;  ///< 文件/图片 chunk ≤1MB

// 验证码有效期（秒）
constexpr int VERIFY_CODE_EXPIRY_SEC = 600;

// 错误码（与客户端 Global.h:ERRORCODES 枚举 + proto Message.proto:ErrorCode 对齐）
constexpr int ERR_SUCCESS = 0;
constexpr int ERR_PARSE_ERROR = 1001;     ///< JSON/Protobuf 解析失败
constexpr int ERR_INVALID_PARAM = 1002;   ///< 参数校验失败
constexpr int ERR_VERIFY_EXPIRED = 1003;
constexpr int ERR_VERIFY_WRONG = 1004;
constexpr int ERR_USER_EXIST = 1005;
constexpr int ERR_PASSWD_ERR = 1006;
constexpr int ERR_USER_NOT_EXIST = 1007;
constexpr int ERR_EMAIL_NOT_MATCH = 1008;
constexpr int ERR_PASSWD_UPDATE = 1009;
constexpr int ERR_RPC_FAILED = 1010;      ///< RPC 调用失败（与客户端 RPCGetFailed 对齐）
constexpr int ERR_DB = 1011;
constexpr int ERR_KICKED = 1013;  ///< 被另一设备登录踢出
constexpr int ERR_BUSY = 1014;    ///< 服务器繁忙（队列满/连接数超限/限流）
constexpr int ERR_RATE_LIMITED = 1015;  ///< 消息发送频率超限
constexpr int ERR_NETWORK = 2;

// 图片 + 撤回/编辑 错误码
constexpr int ERR_RECALL_TIMEOUT     = 4001;  ///< 超过 2 分钟无法撤回
constexpr int ERR_RECALL_NOT_OWNER   = 4002;  ///< 非本人消息无法撤回
constexpr int ERR_EDIT_TIMEOUT       = 4003;  ///< 超过 2 分钟无法编辑
constexpr int ERR_EDIT_NOT_OWNER     = 4004;  ///< 非本人消息无法编辑
constexpr int ERR_EDIT_TOO_LONG      = 4005;  ///< 编辑内容超长
constexpr int ERR_MSG_ALREADY_RECALLED = 4006; ///< 目标消息已撤回
constexpr int ERR_IMAGE_EXPIRED      = 4040;  ///< 图片 7 天过期

#endif // CONST_H

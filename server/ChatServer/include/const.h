/**
 * @file const.h
 * @brief ChatServer 协议与配置常量
 */
#ifndef CONST_H
#define CONST_H

#include <cstdint>

constexpr uint16_t MSG_HELLO = 1000;             ///< 心跳消息
constexpr uint16_t ID_GET_VARIFY_CODE = 1001;    ///< 获取验证码
constexpr uint16_t ID_REGISTER_USER = 1002;      ///< 用户注册
constexpr uint16_t ID_RESET_PWD = 1003;          ///< 重置密码
constexpr uint16_t ID_LOGIN_USER = 1004;         ///< 用户登录
constexpr uint16_t MSG_CHAT_LOGIN = 1005;        ///< 聊天会话登录
constexpr uint16_t MSG_CHAT_TEXT = 1006;         ///< 聊天文本消息
constexpr uint16_t MSG_CHAT_ACK = 1007;          ///< 聊天消息确认
constexpr uint16_t MSG_OFFLINE_ACK = 1008;       ///< 离线消息分页确认
constexpr uint16_t MSG_FILE_REQ = 2001;          ///< 文件传输请求
constexpr uint16_t MSG_FILE_RSP = 2002;          ///< 文件传输响应(断点续传)
constexpr uint16_t MSG_FILE_CHUNK = 2003;        ///< 文件数据分片(旧协议,兼容)
constexpr uint16_t MSG_FILE_ACK = 2004;          ///< 数据块接收确认(旧协议)
constexpr uint16_t MSG_ZEROCOPY_START = 2010;    ///< 零拷贝传输启动请求
constexpr uint16_t MSG_ZEROCOPY_READY = 2011;    ///< 零拷贝传输就绪(服务端已准备好接收文件描述符)
constexpr uint16_t MSG_ZEROCOPY_DATA = 2012;     ///< 零拷贝数据传输(仅发送文件描述符)
constexpr uint16_t MSG_ZEROCOPY_COMPLETE = 2013; ///< 零拷贝传输完成
constexpr uint16_t MSG_ZEROCOPY_ERROR = 2014;    ///< 零拷贝传输错误
constexpr int MAX_CHAT_CONTENT_LEN = 512;        ///< 单条消息最大长度
constexpr int OFFLINE_PAGE_SIZE = 50;            ///< 离线消息每页数量

// 协议头部常量
const int HEAD_ID_LEN = 2;          ///< 消息 ID 字节长度
const int HEAD_DATA_LEN = 4;        ///< 消息体长度字段字节数
const int HEAD_TOTAL_LEN = 6;       ///< 头部总长度（仅包含ID和总长度）
const int MAX_LENGTH = 1024 * 1024; ///< 单包最大长度

// 二进制数据包协议头部（包含JSON长度字段）
const int HEAD_BIN_ID_LEN = 2;                    ///< 二进制包消息 ID 字节长度
const int HEAD_BIN_TOTAL_LEN_FIELD = 4;           ///< 二进制包总长度字段字节数
const int HEAD_BIN_JSON_LEN_FIELD = 4;            ///< JSON数据长度字段字节数
const int HEAD_BIN_TOTAL_LEN = 10;                ///< 二进制包头部总长度（ID + TotalLen + JsonLen）
const int HEAD_BIN_MAX_LENGTH = 1024 * 1024 * 10; ///< 二进制包最大长度（10MB）

#endif // CONST_H

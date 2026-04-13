/**
 * @file const.h
 * @brief ChatServer 协议与配置常量
 */
#ifndef CONST_H
#define CONST_H

#include <cstdint>

constexpr uint16_t MSG_HELLO = 1000;          ///< 心跳消息
constexpr uint16_t ID_GET_VARIFY_CODE = 1001; ///< 获取验证码
constexpr uint16_t ID_REGISTER_USER = 1002;   ///< 用户注册
constexpr uint16_t ID_RESET_PWD = 1003;       ///< 重置密码
constexpr uint16_t ID_LOGIN_USER = 1004;      ///< 用户登录
constexpr uint16_t MSG_CHAT_LOGIN = 1005;     ///< 聊天会话登录
constexpr uint16_t MSG_CHAT_TEXT = 1006;      ///< 聊天文本消息
constexpr uint16_t MSG_CHAT_ACK = 1007;       ///< 聊天消息确认
constexpr int MAX_CHAT_CONTENT_LEN = 512;     ///< 单条消息最大长度

// 协议头部常量
const int HEAD_ID_LEN = 2;          ///< 消息 ID 字节长度
const int HEAD_DATA_LEN = 4;        ///< 消息体长度字段字节数
const int HEAD_TOTAL_LEN = 6;       ///< 头部总长度
const int MAX_LENGTH = 1024 * 1024; ///< 单包最大长度

#endif // CONST_H

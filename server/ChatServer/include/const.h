/**
 * @file const.h
 * @brief ChatServer 协议与配置常量
 */
#ifndef CONST_H
#define CONST_H

#include <chrono>
#include <cstdint>

constexpr uint16_t MSG_HELLO = 1000;             ///< 心跳消息 (0x3E8)
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
constexpr uint16_t MSG_FILE_CHUNK = 2003;        ///< 文件数据分片(Protobuf 消息体)
constexpr uint16_t MSG_FILE_ACK = 2004;          ///< 数据块接收确认(Protobuf 消息体)
constexpr int MAX_CHAT_CONTENT_LEN = 512;        ///< 单条消息最大长度
constexpr int OFFLINE_PAGE_SIZE = 50;            ///< 离线消息每页数量

// 协议头部常量
const int HEAD_ID_LEN = 2;          ///< 消息 ID 字节长度
const int HEAD_DATA_LEN = 4;        ///< 消息体长度字段字节数
const int HEAD_TOTAL_LEN = 6;       ///< 头部总长度（仅包含ID和总长度）
const int MAX_LENGTH = 1024 * 1024; ///< 单包最大长度

// 二进制数据包协议头部（包含JSON长度字段）
// ⚠️ 与 client/QmsrChat/include/Global.h 中的 HEAD_BIN_* 常量保持同步
const int HEAD_BIN_ID_LEN = 2;                    ///< 二进制包消息 ID 字节长度
const int HEAD_BIN_TOTAL_LEN_FIELD = 4;           ///< 二进制包总长度字段字节数
const int HEAD_BIN_JSON_LEN_FIELD = 4;            ///< JSON数据长度字段字节数
const int HEAD_BIN_TOTAL_LEN = 10;                ///< 二进制包头部总长度（ID + TotalLen + JsonLen）
const int HEAD_BIN_MAX_LENGTH = 1024 * 1024 * 10; ///< 二进制包最大长度（10MB）

// 读取超时
constexpr auto kReadTimeout = std::chrono::seconds(30);
constexpr auto kReadCheckInterval = std::chrono::seconds(5);

// 文件传输
constexpr size_t CHUNK_SIZE = 4 * 1024;

// 错误码（与客户端 Global.h:69-83 的 ERRORCODES 枚举对应）
constexpr int ERR_SUCCESS = 0;
constexpr int ERR_JSON_PARSE = 1001;
constexpr int ERR_RPC_FAILED = 1002;
constexpr int ERR_VERIFY_EXPIRED = 1003;
constexpr int ERR_VERIFY_WRONG = 1004;
constexpr int ERR_USER_EXIST = 1005;
constexpr int ERR_PASSWD_ERR = 1006;
constexpr int ERR_USER_NOT_EXIST = 1007;
constexpr int ERR_EMAIL_NOT_MATCH = 1008;
constexpr int ERR_PASSWD_UPDATE = 1009;
constexpr int ERR_DB = 1011;
constexpr int ERR_NETWORK = 2;

/// 判断是否为二进制包（带额外10字节头），当前无二进制包类型
inline bool IsBinaryPacket(uint16_t msg_id)
{
    (void)msg_id;
    return false;
}

#endif // CONST_H

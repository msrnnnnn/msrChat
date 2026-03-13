#ifndef CONST_H
#define CONST_H

#include <cstdint>

constexpr uint16_t MSG_HELLO = 1000;
constexpr uint16_t MSG_CHAT_LOGIN = 1005;
constexpr uint16_t MSG_CHAT_TEXT = 1006;
constexpr uint16_t MSG_CHAT_ACK = 1007;
constexpr int MAX_CHAT_CONTENT_LEN = 512;

// 协议头部常量
const int HEAD_ID_LEN = 2;
const int HEAD_DATA_LEN = 4;
const int HEAD_TOTAL_LEN = 6;
const int MAX_LENGTH = 1024 * 1024;

#endif // CONST_H

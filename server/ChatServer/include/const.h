#ifndef CONST_H
#define CONST_H

// 心跳检测消息ID
const int MSG_ID_HEARTBEAT = 1;

// 聊天服务消息ID
#define MSG_CHAT_LOGIN 1005
#define MSG_CHAT_TEXT 1006

// 协议头部常量
const int HEAD_ID_LEN = 2;    // 消息ID长度
const int HEAD_DATA_LEN = 2;  // 数据长度字段
const int HEAD_TOTAL_LEN = 4; // 头部总长度
const int MAX_LENGTH = 4096;  // 最大包体长度

#endif // CONST_H

#pragma once
#define MAX_LENGTH 8192
#define HEAD_TOTAL_LEN 4
#define HEAD_ID_LEN 2
#define HEAD_DATA_LEN 2

enum MSG_IDS
{
    MSG_CHAT_LOGIN = 1005, // 对应客户端的 ID_CHAT_LOGIN
    MSG_CHAT_TEXT = 1006
};

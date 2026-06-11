#pragma once
/**
 * @file ChatService.h
 * @brief 聊天相关消息处理器
 * @details 文本消息、离线消息 ACK
 */

#include "CSession.h"
#include <string>

class ChatService
{
public:
    static bool HandleChatText(CSession &session, const std::string &body_data);
    static bool HandleOfflineAck(CSession &session, const std::string &body_data);
};

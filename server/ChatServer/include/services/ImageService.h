#pragma once
/**
 * @file ImageService.h
 * @brief 图片 + 撤回/编辑消息处理器
 * @details 图片消息、图片下载、消息撤回、消息编辑
 */

#include "CSession.h"
#include <string>

class ImageService
{
public:
    static bool HandleChatImage(CSession &session, const std::string &body_data);
    static bool HandleImageDownloadReq(CSession &session, const std::string &body_data);
    static bool HandleChatRecall(CSession &session, const std::string &body_data);
    static bool HandleChatEdit(CSession &session, const std::string &body_data);
};

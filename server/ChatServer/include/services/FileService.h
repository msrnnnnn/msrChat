#pragma once
/**
 * @file FileService.h
 * @brief 文件传输相关消息处理器
 * @details 文件请求、响应、分片、确认
 */

#include "CSession.h"
#include <string>
#include <string_view>

class FileService
{
public:
    static bool HandleFileReq(CSession &session, const std::string &body_data);
    static bool HandleFileRsp(CSession &session, const std::string &body_data);
    static bool HandleFileChunk(CSession &session, const std::string &body_data);
    static bool HandleFileChunk(CSession &session, std::string_view body_view);
    static bool HandleFileAck(CSession &session, const std::string &body_data);
};

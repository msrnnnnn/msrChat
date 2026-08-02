#pragma once
/**
 * @file ImageService.h
 * @brief 图片 + 撤回/编辑消息处理器
 * @details 图片消息、图片下载、消息撤回、消息编辑
 */

#include "CSession.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct ImageDownloadState
{
    std::string image_id;
    int64_t total_size = 0;
    int64_t offset = 0;
    int64_t task_id = 0;
    std::weak_ptr<CSession> session;
};

class ImageService
{
public:
    static bool HandleChatImage(CSession &session, const std::string &body_data);
    static bool HandleImageDownloadReq(CSession &session, const std::string &body_data);
    static bool HandleChatRecall(CSession &session, const std::string &body_data);
    static bool HandleChatEdit(CSession &session, const std::string &body_data);
    static void ContinueImageDownload(int64_t task_id);

private:
    static std::mutex _download_mutex;
    static std::unordered_map<int64_t, std::shared_ptr<ImageDownloadState>> _pending_downloads;
};

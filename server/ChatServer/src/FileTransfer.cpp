/**
 * @file FileTransfer.cpp
 * @brief P2P 文件传输管理器实现
 * @details 负责文件传输任务映射与分片管理。
 */
#include "FileTransfer.h"

/**
 * @brief 获取单例实例
 * @return FileTransfer& 全局唯一实例
 */
FileTransfer &FileTransfer::Instance()
{
    static FileTransfer instance;
    return instance;
}

/**
 * @brief 添加传输任务（已有 task_id）
 * @param task_id 任务 ID
 * @param from_uid 发送方用户 ID
 * @param to_uid 接收方用户 ID
 * @param filename 文件名
 * @param total_size 文件总大小
 * @param md5 文件MD5校验值（可选）
 */
void FileTransfer::AddTask(int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size,
                           const std::string &md5)
{
    std::lock_guard<std::mutex> lock(_task_mutex);
    auto task = TaskPool().Acquire(task_id, from_uid, to_uid, filename, total_size, md5);
    _tasks[task_id] = task;
}

/**
 * @brief 获取传输任务
 * @param task_id 任务 ID
 * @return 任务智能指针，不存在返回 nullptr
 */
std::shared_ptr<FileTransferTask> FileTransfer::GetTask(int64_t task_id)
{
    std::lock_guard<std::mutex> lock(_task_mutex);
    auto it = _tasks.find(task_id);
    if (it != _tasks.end())
    {
        return it->second;
    }
    return nullptr;
}

/**
 * @brief 移除传输任务
 * @param task_id 任务 ID
 */
void FileTransfer::RemoveTask(int64_t task_id)
{
    std::lock_guard<std::mutex> lock(_task_mutex);
    _tasks.erase(task_id);
}

/**
 * @brief 按用户 ID 清理传输任务
 * @param uid 用户 ID
 * @details 会话断开时调用，遍历并移除所有与该用户相关的任务
 */
void FileTransfer::RemoveTaskBySession(int uid)
{
    std::lock_guard<std::mutex> lock(_task_mutex);
    for (auto it = _tasks.begin(); it != _tasks.end();)
    {
        if (it->second->GetFromUid() == uid || it->second->GetToUid() == uid)
        {
            it = _tasks.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

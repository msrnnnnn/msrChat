/**
 * @file FileTransfer.cpp
 * @brief P2P 文件传输管理器实现
 * @details 负责文件传输任务映射与分片管理。
 */
#include "FileTransfer.h"

/**
 * @brief 更新传输进度
 * @param size 本次传输大小
 * @details 原子操作递增已传输字节数，达到总量时自动标记为完成
 */
void FileTransferTask::UpdateProgress(int64_t size)
{
    int64_t old_size = _transferred_size.load();
    while (!_transferred_size.compare_exchange_weak(old_size, old_size + size))
    {
    }

    if (_transferred_size >= _total_size)
    {
        _status.store(Status::COMPLETED);
    }
}

/**
 * @brief 判断传输是否完成
 * @return 完成返回 true
 */
bool FileTransferTask::IsCompleted() const
{
    return _transferred_size.load() >= _total_size;
}

/**
 * @brief 设置传输状态
 * @param status 新状态
 */
void FileTransferTask::SetStatus(Status status)
{
    _status.store(status);
}

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
 */
void FileTransfer::AddTask(int64_t task_id, int from_uid, int to_uid,
                           const std::string &filename, int64_t total_size)
{
    std::lock_guard<std::mutex> lock(_task_mutex);
    auto task = TaskPool().Acquire(task_id, from_uid, to_uid, filename, total_size);
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

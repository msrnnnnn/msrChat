/**
 * @file FileTransfer.h
 * @brief 文件传输任务管理与路由
 * @details 管理 P2P 文件传输任务的生命周期，支持对象池复用，
 *          使用互斥锁保护任务映射表，原子变量管理进度与状态。
 */
#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include "ObjectPool.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

/**
 * @class FileTransferTask
 * @brief 单个文件传输任务的状态与进度
 * @details 使用原子变量存储传输进度和状态，支持跨线程读取。
 *          可通过 ObjectPool 复用，降低频繁创建/销毁开销。
 */
class FileTransferTask
{
public:
    enum class Status : uint8_t
    {
        PENDING = 0,
        TRANSFERRING = 1,
        COMPLETED = 2
    };

    FileTransferTask()
        : _task_id(0),
          _from_uid(0),
          _to_uid(0),
          _total_size(0),
          _transferred_size(0),
          _status(Status::PENDING)
    {
    }

    FileTransferTask(int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size)
        : _task_id(task_id),
          _from_uid(from_uid),
          _to_uid(to_uid),
          _filename(filename),
          _total_size(total_size),
          _transferred_size(0),
          _status(Status::PENDING)
    {
    }

    void SetPool(ObjectPool<FileTransferTask> *pool) noexcept
    {
        _pool = pool;
    }

    void SetPool(std::nullptr_t) noexcept
    {
        _pool = nullptr;
    }

    void Reset() noexcept
    {
        _task_id = 0;
        _from_uid = 0;
        _to_uid = 0;
        _filename.clear();
        _total_size = 0;
        _transferred_size.store(0, std::memory_order_relaxed);
        _status.store(Status::PENDING, std::memory_order_relaxed);
        _is_image = false;
        _image_id.clear();
        _target_offline = false;
    }

    void Init(int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size) noexcept
    {
        _task_id = task_id;
        _from_uid = from_uid;
        _to_uid = to_uid;
        _filename = filename;
        _total_size = total_size;
        _transferred_size.store(0, std::memory_order_relaxed);
        _status.store(Status::PENDING, std::memory_order_relaxed);
        _is_image = false;
        _image_id.clear();
        _target_offline = false;
    }

    int64_t GetTaskId() const
    {
        return _task_id;
    }
    int GetFromUid() const
    {
        return _from_uid;
    }
    int GetToUid() const
    {
        return _to_uid;
    }
    const std::string &GetFilename() const
    {
        return _filename;
    }
    int64_t GetTotalSize() const
    {
        return _total_size;
    }
    int64_t GetTransferredSize() const
    {
        return _transferred_size.load();
    }
    Status GetStatus() const
    {
        return _status.load();
    }

    void UpdateProgress(int64_t size);
    bool IsCompleted() const;
    void SetStatus(Status status);

    void SetIsImage(bool is_image) { _is_image = is_image; }
    bool IsImage() const { return _is_image; }
    const std::string &GetImageId() const { return _image_id; }
    void SetImageId(const std::string &id) { _image_id = id; }

    void SetTargetOffline(bool offline) { _target_offline = offline; }
    bool IsTargetOffline() const { return _target_offline; }

private:
    int64_t _task_id = 0;
    int _from_uid = 0;
    int _to_uid = 0;
    std::string _filename;
    int64_t _total_size = 0;
    std::atomic<int64_t> _transferred_size{0};
    std::atomic<Status> _status{Status::PENDING};
    ObjectPool<FileTransferTask> *_pool = nullptr;
    bool _is_image = false;
    std::string _image_id;
    bool _target_offline = false;
};

/**
 * @class FileTransfer
 * @brief 文件传输任务路由管理器（单例）
 * @details 管理所有进行中的文件传输任务。
 *          使用 std::map + std::mutex 保护任务表，原子变量分配任务 ID。
 *          支持通过会话 UID 批量移除任务（用于会话断开清理）。
 */
class FileTransfer
{
public:
    /**
     * @brief 获取单例实例
     */
    static FileTransfer &Instance();

    static ObjectPool<FileTransferTask> &TaskPool()
    {
        static ObjectPool<FileTransferTask> pool(1000, 128);
        return pool;
    }

    /**
     * @brief 创建并注册文件传输任务（由外部指定 task_id）
     * @param task_id 任务 ID（来自客户端请求）
     * @param from_uid 发送方 UID
     * @param to_uid 接收方 UID
     * @param filename 文件名
     * @param total_size 文件总大小
     */
    void AddTask(int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size);

    /**
     * @brief 按 task_id 查询传输任务
     * @param task_id 任务 ID
     * @return 任务对象指针，不存在时返回 nullptr
     */
    std::shared_ptr<FileTransferTask> GetTask(int64_t task_id);

    /**
     * @brief 按 task_id 移除传输任务
     */
    void RemoveTask(int64_t task_id);

    /**
     * @brief 移除指定用户的所有传输任务（用于会话断开清理）
     * @param uid 用户 UID
     */
    void RemoveTaskBySession(int uid);


    FileTransfer(const FileTransfer &) = delete;
    FileTransfer &operator=(const FileTransfer &) = delete;

private:
    FileTransfer() = default;
    ~FileTransfer() = default;

    std::map<int64_t, std::shared_ptr<FileTransferTask>> _tasks;
    std::mutex _task_mutex;
    std::atomic<int64_t> _task_id_allocator{1};
};

#endif
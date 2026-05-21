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

constexpr size_t LARGE_FILE_THRESHOLD = 1024 * 1024;

class FileTransferTask
{
public:
    enum class Status : uint8_t
    {
        PENDING = 0,
        TRANSFERRING = 1,
        COMPLETED = 2,
        FAILED = 3,
        PAUSED = 4
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

private:
    int64_t _task_id = 0;
    int _from_uid = 0;
    int _to_uid = 0;
    std::string _filename;
    int64_t _total_size = 0;
    std::atomic<int64_t> _transferred_size{0};
    std::atomic<Status> _status{Status::PENDING};
    ObjectPool<FileTransferTask> *_pool = nullptr;
};

class FileTransfer
{
public:
    static FileTransfer &Instance();

    static ObjectPool<FileTransferTask> &TaskPool()
    {
        static ObjectPool<FileTransferTask> pool(1000, 128);
        return pool;
    }

    int64_t CreateTask(boost::asio::io_context &ioc, int from_uid, int to_uid, const std::string &filename, int64_t total_size);
    // 由外部指定 task_id 创建路由记录（用于 P2P 转发）
    void AddTask(int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size);
    std::shared_ptr<FileTransferTask> GetTask(int64_t task_id);
    void RemoveTask(int64_t task_id);
    void RemoveTaskBySession(int uid);

    bool SendFileChunked(int fd, std::function<bool(const char *, size_t)> send_callback, int64_t offset, int64_t size);

    std::string CalculateMD5(const std::string &filepath);
    std::string CalculateChunkMD5(const char *data, size_t len);

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
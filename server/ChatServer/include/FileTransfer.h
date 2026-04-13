#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

constexpr size_t CHUNK_SIZE = 64 * 1024;
constexpr size_t LARGE_FILE_THRESHOLD = 1024 * 1024;

class FileTransferTask
{
public:
    enum class Status : uint8_t {
        PENDING = 0,
        TRANSFERRING = 1,
        COMPLETED = 2,
        FAILED = 3,
        PAUSED = 4
    };
    
    FileTransferTask(int64_t task_id, int from_uid, int to_uid, const std::string& filename, int64_t total_size)
        : _task_id(task_id), _from_uid(from_uid), _to_uid(to_uid), 
          _filename(filename), _total_size(total_size), _transferred_size(0), _status(Status::PENDING) {}
    
    int64_t GetTaskId() const { return _task_id; }
    int GetFromUid() const { return _from_uid; }
    int GetToUid() const { return _to_uid; }
    const std::string& GetFilename() const { return _filename; }
    int64_t GetTotalSize() const { return _total_size; }
    int64_t GetTransferredSize() const { return _transferred_size.load(); }
    Status GetStatus() const { return _status.load(); }
    
    void UpdateProgress(int64_t size);
    bool IsCompleted() const;
    void SetStatus(Status status);
    
private:
    int64_t _task_id;
    int _from_uid;
    int _to_uid;
    std::string _filename;
    int64_t _total_size;
    std::atomic<int64_t> _transferred_size;
    std::atomic<Status> _status;
};

class FileTransfer
{
public:
    static FileTransfer& Instance();
    
    int64_t CreateTask(int from_uid, int to_uid, const std::string& filename, int64_t total_size);
    std::shared_ptr<FileTransferTask> GetTask(int64_t task_id);
    void RemoveTask(int64_t task_id);
    
    bool SendFile(int fd, int client_fd, int64_t offset, int64_t size);
    bool SendFileLarge(int fd, int client_fd, int64_t offset, int64_t size);
    
    std::string CalculateMD5(const std::string& filepath);
    std::string CalculateChunkMD5(const char* data, size_t len);
    
    FileTransfer(const FileTransfer&) = delete;
    FileTransfer& operator=(const FileTransfer&) = delete;

private:
    FileTransfer() = default;
    ~FileTransfer() = default;
    
    std::map<int64_t, std::shared_ptr<FileTransferTask>> _tasks;
    std::mutex _mutex;
    std::atomic<int64_t> _task_id_allocator{1};
};

#endif

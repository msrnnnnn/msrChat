#include "FileTransfer.h"
#include "const.h"
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

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

bool FileTransferTask::IsCompleted() const
{
    return _transferred_size.load() >= _total_size;
}

void FileTransferTask::SetStatus(Status status)
{
    _status.store(status);
}

FileTransfer &FileTransfer::Instance()
{
    static FileTransfer instance;
    return instance;
}

int64_t FileTransfer::CreateTask(int from_uid, int to_uid, const std::string &filename, int64_t total_size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    int64_t task_id = _task_id_allocator++;

    auto task = TaskPool().Acquire(task_id, from_uid, to_uid, filename, total_size);
    _tasks[task_id] = task;

    return task_id;
}

void FileTransfer::AddTask(int64_t task_id, int from_uid, int to_uid,
                           const std::string &filename, int64_t total_size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto task = TaskPool().Acquire(task_id, from_uid, to_uid, filename, total_size);
    _tasks[task_id] = task;
}

std::shared_ptr<FileTransferTask> FileTransfer::GetTask(int64_t task_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _tasks.find(task_id);
    if (it != _tasks.end())
    {
        return it->second;
    }
    return nullptr;
}

void FileTransfer::RemoveTask(int64_t task_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _tasks.erase(task_id);
}

bool FileTransfer::SendFileChunked(
    int fd, std::function<bool(const char *, size_t)> send_callback, int64_t offset, int64_t size)
{
    char buffer[CHUNK_SIZE];
    ssize_t total_sent = 0;

    if (lseek(fd, offset, SEEK_SET) == -1)
    {
        return false;
    }

    while (total_sent < size)
    {
        ssize_t to_read = std::min(static_cast<int64_t>(CHUNK_SIZE), size - total_sent);
        ssize_t bytes_read = read(fd, buffer, to_read);

        if (bytes_read <= 0)
        {
            break;
        }

        if (!send_callback(buffer, static_cast<size_t>(bytes_read)))
        {
            return false;
        }

        total_sent += bytes_read;
    }

    return total_sent == size;
}

std::string FileTransfer::CalculateMD5(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        return "";
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);

    char buffer[CHUNK_SIZE];
    while (file.read(buffer, sizeof(buffer)))
    {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }
    if (file.gcount() > 0)
    {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    char md5_str[33];
    for (int i = 0; i < 16; ++i)
    {
        sprintf(md5_str + i * 2, "%02x", digest[i]);
    }

    return std::string(md5_str, 32);
}

FileSender::FileSender(
    int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size, int fd,
    SendCallback send_cb, ProgressCallback progress_cb, CompleteCallback complete_cb)
    : _task_id(task_id),
      _from_uid(from_uid),
      _to_uid(to_uid),
      _filename(filename),
      _total_size(total_size),
      _fd(fd),
      _send_callback(std::move(send_cb)),
      _progress_callback(std::move(progress_cb)),
      _complete_callback(std::move(complete_cb))
{
}

void FileSender::Start()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_running)
    {
        return;
    }
    _running = true;
    _stopped.store(false);
    SendNextChunk();
}

void FileSender::Stop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _stopped.store(true);
    _running = false;
    _cv.notify_all();
}

void FileSender::SendNextChunk()
{
    if (_stopped.load())
    {
        return;
    }

    if (_sent_size >= _total_size)
    {
        if (_complete_callback)
        {
            _complete_callback(_task_id, true, "transfer completed");
        }
        return;
    }

    char buffer[CHUNK_SIZE];
    ssize_t to_read = std::min(static_cast<int64_t>(CHUNK_SIZE), _total_size - _sent_size);
    ssize_t bytes_read = read(_fd, buffer, to_read);

    if (bytes_read <= 0)
    {
        if (_complete_callback)
        {
            _complete_callback(_task_id, false, "read error");
        }
        return;
    }

    SendChunkData(buffer, static_cast<size_t>(bytes_read));
}

void FileSender::SendChunkData(const char *data, size_t len)
{
    nlohmann::json header;
    header["task_id"] = _task_id;
    header["offset"] = _sent_size;
    header["size"] = len;

    std::string header_str = header.dump();
    std::string msg;
    msg.reserve(header_str.size() + len);
    msg = header_str + std::string(data, len);

    bool sent = _send_callback(msg, MSG_FILE_CHUNK);
    if (!sent)
    {
        if (_complete_callback)
        {
            _complete_callback(_task_id, false, "send failed");
        }
        return;
    }

    _sent_size += len;

    int progress = static_cast<int>((_sent_size * 100) / _total_size);
    if (_progress_callback)
    {
        _progress_callback(_task_id, progress, _sent_size, _total_size);
    }

    std::weak_ptr<FileSender> weak_self = shared_from_this();
    std::thread(
        [weak_self, this]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto self = weak_self.lock();
            if (self)
            {
                SendNextChunk();
            }
        })
        .detach();
}

void FileSender::OnChunkAck(bool success, const std::string &message)
{
    if (!success)
    {
        if (_complete_callback)
        {
            _complete_callback(_task_id, false, message);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if (_sent_size >= _total_size)
    {
        if (_complete_callback)
        {
            _complete_callback(_task_id, true, "transfer completed");
        }
    }
}

std::string FileTransfer::CalculateChunkMD5(const char *data, size_t len)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, data, len);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    char md5_str[33];
    for (int i = 0; i < 16; ++i)
    {
        sprintf(md5_str + i * 2, "%02x", digest[i]);
    }

    return std::string(md5_str, 32);
}

#include "FileTransfer.h"
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <openssl/md5.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
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

    auto task = std::make_shared<FileTransferTask>(task_id, from_uid, to_uid, filename, total_size);
    _tasks[task_id] = task;

    return task_id;
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

bool FileTransfer::SendFile(int fd, int client_fd, int64_t offset, int64_t size)
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

        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read)
        {
            ssize_t sent = write(client_fd, buffer + bytes_sent, bytes_read - bytes_sent);
            if (sent <= 0)
            {
                return false;
            }
            bytes_sent += sent;
        }

        total_sent += bytes_read;
    }

    return total_sent == size;
}

bool FileTransfer::SendFileLarge(int fd, int client_fd, int64_t offset, int64_t size)
{
    if (size < LARGE_FILE_THRESHOLD)
    {
        return SendFile(fd, client_fd, offset, size);
    }

    void *addr = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, offset);
    if (addr == MAP_FAILED)
    {
        return SendFile(fd, client_fd, offset, size);
    }

    off_t send_offset = offset;
    size_t remaining = size;

    while (remaining > 0)
    {
        ssize_t sent = sendfile(client_fd, fd, &send_offset, remaining);
        if (sent <= 0)
        {
            munmap(addr, size);
            return false;
        }
        remaining -= static_cast<size_t>(sent);
    }

    munmap(addr, size);
    return true;
}

std::string FileTransfer::CalculateMD5(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        return "";
    }

    MD5_CTX ctx;
    MD5_Init(&ctx);

    char buffer[CHUNK_SIZE];
    while (file.read(buffer, sizeof(buffer)))
    {
        MD5_Update(&ctx, buffer, file.gcount());
    }
    if (file.gcount() > 0)
    {
        MD5_Update(&ctx, buffer, file.gcount());
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);

    char md5_str[33];
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(md5_str + i * 2, "%02x", digest[i]);
    }

    return std::string(md5_str, 32);
}

std::string FileTransfer::CalculateChunkMD5(const char *data, size_t len)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char *>(data), len, digest);

    char md5_str[33];
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(md5_str + i * 2, "%02x", digest[i]);
    }

    return std::string(md5_str, 32);
}

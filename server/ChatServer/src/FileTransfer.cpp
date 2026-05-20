/**
 * @file FileTransfer.cpp
 * @brief P2P 文件传输管理器实现
 * @details 负责文件传输任务映射、分片发送与 MD5 校验。
 */
#include "FileTransfer.h"
#include "const.h"
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>

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
 * @brief 创建传输任务
 * @param ioc Boost ASIO io_context
 * @param from_uid 发送方用户 ID
 * @param to_uid 接收方用户 ID
 * @param filename 文件名
 * @param total_size 文件总大小
 * @return 任务 ID
 */
int64_t FileTransfer::CreateTask(boost::asio::io_context &ioc, int from_uid, int to_uid, const std::string &filename, int64_t total_size)
{
    (void)ioc;  // 未使用参数
    int64_t task_id;
    std::shared_ptr<FileTransferTask> task;
    {
        std::lock_guard<std::mutex> lock(_task_mutex);
        task_id = _task_id_allocator.fetch_add(1);
        task = TaskPool().Acquire(task_id, from_uid, to_uid, filename, total_size);
        _tasks[task_id] = task;
    }

    return task_id;
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

/**
 * @brief 分片发送文件（同步模式）
 * @param fd 文件描述符
 * @param send_callback 发送回调，返回 true 表示发送成功
 * @param offset 起始偏移
 * @param size 发送总字节数
 * @return 全部发送成功返回 true
 */
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

/**
 * @brief 计算文件 MD5
 * @param filepath 文件路径
 * @return MD5 十六进制字符串，空字符串表示失败
 */
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
    boost::asio::io_context &ioc, int64_t task_id, int from_uid, int to_uid, const std::string &filename, int64_t total_size, int fd,
    SendCallback send_cb, ProgressCallback progress_cb, CompleteCallback complete_cb)
    : _task_id(task_id),
      _from_uid(from_uid),
      _to_uid(to_uid),
      _filename(filename),
      _total_size(total_size),
      _fd(fd),
      _send_callback(std::move(send_cb)),
      _progress_callback(std::move(progress_cb)),
      _complete_callback(std::move(complete_cb)),
      _timer(ioc)
{
}

/**
 * @brief 启动分片发送
 */
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

/**
 * @brief 停止分片发送
 */
void FileSender::Stop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _stopped.store(true);
    _running = false;
    _cv.notify_all();
}

/**
 * @brief 发送下一个分片
 * @details 读取文件指定位置的数据，通过回调发送后启动定时器递归发送下一片
 */
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

/**
 * @brief 发送分片数据
 * @param data 数据指针
 * @param len 数据长度
 * @details 组装 JSON 头（task_id/offset/size）和二进制负载后发送
 */
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
    _timer.expires_after(std::chrono::milliseconds(10));
    _timer.async_wait(
        [weak_self, this](const boost::system::error_code &ec)
        {
            if (ec || _stopped.load()) return;
            auto self = weak_self.lock();
            if (self) SendNextChunk();
        });
}

/**
 * @brief 处理分片 ACK
 * @param success 是否成功
 * @param message 附加消息
 */
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

/**
 * @brief 计算分片数据 MD5
 * @param data 数据指针
 * @param len 数据长度
 * @return MD5 十六进制字符串
 */
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

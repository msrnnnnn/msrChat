/**
 * @file CSession.h
 * @brief TCP 会话与协议收发定义
 * @details 包含协议收包节点、发包节点以及会话类声明。
 */
#pragma once
#include "FileDescriptor.h"
#include "FileTransferState.h"
#include "ObjectPool.h"
#include "OfflineSendState.h"
#include "const.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class CServer;

class RecvNode
{
public:
    uint16_t _msg_id;
    uint32_t _total_len;
    char *_data;
    std::vector<char> _buffer;

    RecvNode()
        : _msg_id(0),
          _total_len(0),
          _data(nullptr),
          _buffer(HEAD_TOTAL_LEN + 1)
    {
        _data = _buffer.data();
    }

    void SetPool(ObjectPool<RecvNode> *pool) noexcept
    {
        _pool = pool;
    }

    void SetPool(std::nullptr_t) noexcept
    {
        _pool = nullptr;
    }

    void Reset() noexcept
    {
        _msg_id = 0;
        _total_len = 0;
        _data = _buffer.data();
        if (!_buffer.empty())
        {
            _buffer[0] = '\0';
        }
    }

    void Reset(uint32_t max_len, uint16_t msg_id)
    {
        _msg_id = msg_id;
        _total_len = max_len;
        if (_buffer.size() < static_cast<std::size_t>(_total_len) + 1)
        {
            _buffer.resize(static_cast<std::size_t>(_total_len) + 1);
        }
        _data = _buffer.data();
        _data[_total_len] = '\0';
    }

    void Clear() noexcept
    {
        if (!_buffer.empty())
        {
            ::memset(_buffer.data(), 0, _buffer.size());
            _data = _buffer.data();
        }
    }

private:
    ObjectPool<RecvNode> *_pool = nullptr;
};

class SendNode
{
public:
    uint16_t _msg_id;
    uint32_t _total_len;
    char *_data;
    std::vector<char> _buffer;

    SendNode()
        : _msg_id(0),
          _total_len(0),
          _data(nullptr),
          _buffer(HEAD_TOTAL_LEN)
    {
        _data = _buffer.data();
    }

    void SetPool(ObjectPool<SendNode> *pool) noexcept
    {
        _pool = pool;
    }

    void SetPool(std::nullptr_t) noexcept
    {
        _pool = nullptr;
    }

    void Reset() noexcept
    {
        _msg_id = 0;
        _total_len = 0;
        _data = _buffer.data();
    }

    void Reset(const std::string &msg, uint16_t msg_id)
    {
        _msg_id = msg_id;
        _total_len = static_cast<uint32_t>(msg.length());
        if (_buffer.size() < static_cast<std::size_t>(_total_len) + HEAD_TOTAL_LEN)
        {
            _buffer.resize(static_cast<std::size_t>(_total_len) + HEAD_TOTAL_LEN);
        }
        _data = _buffer.data();
        // TODO: replace boost::asio::detail::socket_ops with htons/ntohs (Boost internal API, unstable)
        uint16_t net_msg_id = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
        memcpy(_data, &net_msg_id, 2);
        uint32_t net_len =
            boost::asio::detail::socket_ops::host_to_network_long(static_cast<unsigned long>(_total_len));
        memcpy(_data + 2, &net_len, 4);
        if (_total_len > 0)
        {
            memcpy(_data + 6, msg.data(), _total_len);
        }
    }

private:
    ObjectPool<SendNode> *_pool = nullptr;
};

class CServer;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
    static ObjectPool<RecvNode> &RecvNodePool()
    {
        static ObjectPool<RecvNode> pool(10000, 1024);
        return pool;
    }

    static ObjectPool<SendNode> &SendNodePool()
    {
        static ObjectPool<SendNode> pool(10000, 1024);
        return pool;
    }

    CSession(boost::asio::io_context &ioc, std::shared_ptr<CServer> server);
    ~CSession();

    void Close();
    void Start();

    void Send(const std::string &msg, short msg_id);

    void StartFileSend(int64_t task_id, const std::string &filepath);
    void SendNextFileChunk();
    void SendNextOfflinePage();

    bool HasOfflineMessagesToSend() const
    {
        std::lock_guard<std::recursive_mutex> lock(_offline_mutex);
        return _offline_send_state.sending && _offline_send_state.sent_count < _offline_send_state.total_count;
    }

    void ContinueOfflineSend();

    std::string GetUuid() const
    {
        return _uuid;
    }
    int GetUserUid() const
    {
        return _user_uid;
    }

    boost::asio::ip::tcp::socket &GetSocket()
    {
        return _socket;
    }

    boost::asio::strand<boost::asio::io_context::executor_type> &GetStrand()
    {
        return _strand;
    }

    void OnLoginValidated(int uid, bool valid);

    std::shared_ptr<CServer> GetServer() const
    {
        return _server.lock();
    }

    void ContinueReading()
    {
        bool expected = false;
        if (!_read_active.compare_exchange_strong(expected, true))
        {
            return;
        }
        auto self = shared_from_this();
        boost::asio::post(_strand, [this, self]() { AsyncReadHead(); });
    }

    bool IsClosed() const
    {
        return _b_closed.load();
    }

    bool TrySetLoginInProgress(bool &expected)
    {
        return _login_in_progress.compare_exchange_strong(expected, true);
    }

    void PrepareFileReceive(int64_t task_id, int to_uid, const std::string &filename, int64_t total_size)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        _file_recv_state.task_id = task_id;
        _file_recv_state.from_uid = _user_uid;
        _file_recv_state.to_uid = to_uid;
        _file_recv_state.filename = filename;
        _file_recv_state.total_size = total_size;
        _file_recv_state.received_size = 0;
        _file_recv_state.data.clear();
        _file_recv_state.data.reserve(static_cast<size_t>(total_size));
        _file_recv_state.transfer_ready = true;
    }

    bool IsFileTransferReady(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        return _file_recv_state.transfer_ready && _file_recv_state.task_id == task_id;
    }

    void AppendFileChunk(int64_t task_id, const std::string &chunk_data)
    {
        AppendFileChunk(task_id, chunk_data.data(), chunk_data.size());
    }

    void AppendFileChunk(int64_t task_id, const char *data, std::size_t size)
    {
        if (size == 0 || data == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_recv_state.task_id == task_id)
        {
            const auto old_size = _file_recv_state.data.size();
            _file_recv_state.data.resize(old_size + size);
            std::memcpy(_file_recv_state.data.data() + old_size, data, size);
            _file_recv_state.received_size += size;
        }
    }

    void AppendFileChunk(int64_t task_id, std::string_view chunk_view)
    {
        AppendFileChunk(task_id, chunk_view.data(), chunk_view.size());
    }

    int GetFileTransferProgress(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_recv_state.task_id == task_id && _file_recv_state.total_size > 0)
        {
            return static_cast<int>((_file_recv_state.received_size * 100) / _file_recv_state.total_size);
        }
        return 0;
    }

    bool IsFileTransferComplete(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        return _file_recv_state.task_id == task_id && _file_recv_state.received_size >= _file_recv_state.total_size;
    }

    void FinishFileReceive(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_recv_state.task_id == task_id)
        {
            _file_recv_state.transfer_ready = false;
            _file_recv_state.data.clear();
        }
    }

    int64_t GetReceivedFileSize(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_recv_state.task_id == task_id)
        {
            return _file_recv_state.received_size;
        }
        return 0;
    }

    void StartFileSend(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_send_state.sending)
            return;
        if (_file_send_state.task_id == task_id)
        {
            _file_send_state.sending = true;
        }
    }

    void UpdateFileSendProgress(int64_t task_id, int64_t received)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_send_state.task_id == task_id)
        {
            _file_send_state.sent_size = received;
            SendNextFileChunk();
        }
    }

    void FinishFileSend(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_send_state.task_id == task_id)
        {
            if (_file_send_state.fd >= 0)
            {
                close(_file_send_state.fd);
                _file_send_state.fd = -1;
            }
            _file_send_state.sending = false;
            _file_send_state.task_id = 0;
        }
    }

    void CancelFileSend(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_send_state.task_id == task_id)
        {
            if (_file_send_state.fd >= 0)
            {
                close(_file_send_state.fd);
                _file_send_state.fd = -1;
            }
            _file_send_state.sending = false;
            _file_send_state.task_id = 0;
        }
    }

private:
    friend class CServer;
    friend class MessageDispatcher;

    void ResetReadDeadline();
    void ScheduleReadDeadlineCheck();

    /**
     * @brief 完整会话清理（传输层错误时调用）
     * @param ec boost 错误码，eof 视为正常断开
     */
    void CleanupSession(const boost::system::error_code &ec = {});

    /**
     * @brief 部分会话清理（协议错误时调用）
     * @param error_msg 错误信息，用于日志记录
     */
    void TerminateSession(const std::string &error_msg = "");

    void AsyncReadHead();
    void AsyncReadBody(int total_len);

    void AsyncWriteMsg();

    std::string _uuid;
    int _user_uid = 0;

    boost::asio::ip::tcp::socket _socket;
    boost::asio::steady_timer _read_deadline;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    std::chrono::steady_clock::time_point _expiry_time;

    std::shared_ptr<RecvNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;

    std::deque<std::shared_ptr<SendNode>> _send_queue;
    std::atomic<bool> _is_writing{false};
    std::atomic<bool> _b_closed{false};
    std::atomic<bool> _read_active{false};
    std::atomic<bool> _login_in_progress{false};

    FileTransferState _file_recv_state;
    FileSendState _file_send_state;
    OfflineSendState _offline_send_state;

    std::mutex _file_mutex;
    mutable std::recursive_mutex _offline_mutex;

    std::weak_ptr<CServer> _server;
};

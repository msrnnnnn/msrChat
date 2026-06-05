/**
 * @file CSession.h
 * @brief TCP 会话与协议收发定义
 * @details 包含协议收包节点、发包节点以及会话类声明。
 */
#ifndef CSESSION_H
#define CSESSION_H
#include "FileDescriptor.h"
#include "FileTransferState.h"
#include "ObjectPool.h"
#include "OfflineSendState.h"
#include "const.h"
#include "Protocol/PacketNode.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

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
        return _closed.load();
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

    void FinishFileReceive(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        if (_file_recv_state.task_id == task_id)
        {
            _file_recv_state.transfer_ready = false;
            _file_recv_state.data.clear();
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
    std::atomic<bool> _closed{false};
    std::atomic<bool> _read_active{false};
    std::atomic<bool> _login_in_progress{false};

    FileTransferState _file_recv_state;
    OfflineSendState _offline_send_state;

    std::mutex _file_mutex;
    mutable std::recursive_mutex _offline_mutex;

    std::weak_ptr<CServer> _server;
};

#endif // CSESSION_H

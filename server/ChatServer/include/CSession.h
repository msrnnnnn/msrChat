#pragma once
/**
 * @file CSession.h
 * @brief TCP 会话与协议收发定义
 * @details 包含协议收包节点、发包节点以及会话类声明。
 */
#ifndef CSESSION_H
#define CSESSION_H
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

    /**
     * @brief 关闭会话并清理资源
     */
    void Close();

    /**
     * @brief 启动会话，开始异步读取协议头部
     */
    void Start();

    /**
     * @brief 向对端发送消息
     * @param msg 消息体数据
     * @param msg_id 消息类型 ID
     * @details 消息加入发送队列，由 strand 保证串行写入顺序
     */
    void Send(const std::string &msg, uint16_t msg_id);

    /**
     * @brief 发送离线消息的下一页
     */
    void SendNextOfflinePage();

    /**
     * @brief 检查是否还有离线消息待发送
     * @return true 表示还有未发送完的离线消息
     */
    bool HasOfflineMessagesToSend() const
    {
        std::lock_guard<std::recursive_mutex> lock(_offline_mutex);
        return _offline_send_state.sending && _offline_send_state.sent_count < _offline_send_state.total_count;
    }

    /**
     * @brief 继续发送剩余的离线消息
     */
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

    /**
     * @brief 登录验证回调：成功则绑定 UID，失败则关闭连接
     * @param uid 用户 UID
     * @param valid 验证是否通过
     */
    void OnLoginValidated(int uid, bool valid);

    std::shared_ptr<CServer> GetServer() const
    {
        return _server.lock();
    }

    /**
     * @brief 通过 CAS 确保同一时刻只有一个读操作在进行
     * @details 使用 _read_active 原子标志位防止并发调用 AsyncReadHead
     */
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

    /**
     * @brief 会话级 CAS 登录互斥：确保同一会话只有一个登录请求在处理
     * @param expected 引用传出当前值，调用前应设为 false
     * @return 是否成功获取登录处理权
     */
    bool TrySetLoginInProgress(bool &expected)
    {
        return _login_in_progress.compare_exchange_strong(expected, true);
    }

    /**
     * @brief 初始化文件接收状态，准备接收文件数据
     * @param task_id 文件传输任务 ID
     * @param to_uid 接收方 UID
     * @param filename 文件名
     * @param total_size 文件总大小
     * @details 设置 _file_recv_state 并预分配内存，由 _file_mutex 保护
     */
    void PrepareFileReceive(int64_t task_id, int64_t total_size)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        _file_recv_state.task_id = task_id;
        _file_recv_state.data.clear();
        _file_recv_state.data.reserve(static_cast<size_t>(total_size));
        _file_recv_state.transfer_ready = true;
    }

    bool IsFileTransferReady(int64_t task_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);
        return _file_recv_state.transfer_ready && _file_recv_state.task_id == task_id;
    }

    /**
     * @brief 追加文件数据分片到接收缓冲区（std::string 重载）
     * @param task_id 文件传输任务 ID（不匹配则忽略）
     * @param chunk_data 分片数据
     * @details 在 _file_mutex 保护下追加数据到 _file_recv_state.data
     */
    void AppendFileChunk(int64_t task_id, const std::string &chunk_data)
    {
        AppendFileChunk(task_id, chunk_data.data(), chunk_data.size());
    }

    /**
     * @brief 追加文件数据分片（原始指针重载）
     * @param task_id 文件传输任务 ID
     * @param data 数据指针
     * @param size 数据大小
     */
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
        }
    }

    /**
     * @brief 追加文件数据分片（string_view 重载）
     * @param task_id 文件传输任务 ID
     * @param chunk_view 分片数据视图
     */
    void AppendFileChunk(int64_t task_id, std::string_view chunk_view)
    {
        AppendFileChunk(task_id, chunk_view.data(), chunk_view.size());
    }

    /**
     * @brief 完成文件接收，清理接收状态
     * @param task_id 文件传输任务 ID
     * @details 重置 transfer_ready 并清空已接收的数据缓冲区
     */
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

    /**
     * @brief 异步读取消息头部（msg_id + body_length）
     * @details 通过 strand 串行化，读取固定 6 字节后调用 AsyncReadBody
     */
    void AsyncReadHead();

    /**
     * @brief 异步读取消息体
     * @param total_len 消息体长度
     */
    void AsyncReadBody(int total_len);

    /**
     * @brief 异步写入发送队列中的下一条消息
     * @details 通过 CAS 标志 _is_writing 保证同一时刻只有一个写操作
     */
    void AsyncWriteMsg();

    void ScheduleWriteDeadlineCheck();

    std::string _uuid;
    int _user_uid = 0;

    boost::asio::ip::tcp::socket _socket;
    boost::asio::steady_timer _read_deadline;
    boost::asio::steady_timer _write_deadline;
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    std::chrono::steady_clock::time_point _expiry_time;
    std::chrono::steady_clock::time_point _last_write_time;

    std::shared_ptr<RecvNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;

    std::deque<std::shared_ptr<SendNode>> _send_queue;
    static constexpr size_t MAX_SEND_QUEUE = 1000;
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

/**
 * @file CSession.h
 * @brief TCP 会话与协议收发定义
 * @details 包含协议收包节点、发包节点以及会话类声明。
 */
#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

class CServer;

/**
 * @class RecvNode
 * @brief 接收缓冲节点
 */
class RecvNode
{
public:
    uint16_t _msg_id;   ///< 消息 ID
    uint32_t _total_len; ///< 消息体长度
    char *_data;        ///< 数据缓冲

    /**
     * @brief 构造函数
     * @param max_len 最大消息长度
     * @param msg_id 消息 ID
     */
    RecvNode(uint32_t max_len, uint16_t msg_id)
        : _total_len(max_len),
          _msg_id(msg_id)
    {
        _data = new char[_total_len + 1]();
    }
    /**
     * @brief 析构函数
     */
    ~RecvNode()
    {
        delete[] _data;
    }
    /**
     * @brief 清空缓冲区
     */
    void Clear()
    {
        ::memset(_data, 0, _total_len + 1);
    }
};

/**
 * @class SendNode
 * @brief 发送缓冲节点
 */
class SendNode
{
public:
    uint16_t _msg_id;    ///< 消息 ID
    uint32_t _total_len; ///< 消息体长度
    char *_data;         ///< 数据缓冲

    /**
     * @brief 构造函数
     * @param msg 发送数据
     * @param msg_id 消息 ID
     */
    SendNode(const std::string &msg, uint16_t msg_id)
        : _msg_id(msg_id),
          _total_len(static_cast<uint32_t>(msg.length()))
    {
        _data = new char[_total_len + 6]();
        uint16_t net_msg_id = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
        memcpy(_data, &net_msg_id, 2);
        uint32_t net_len = boost::asio::detail::socket_ops::host_to_network_long(static_cast<unsigned long>(_total_len));
        memcpy(_data + 2, &net_len, 4);
        if (_total_len > 0)
        {
            memcpy(_data + 6, msg.data(), _total_len);
        }
    }
    /**
     * @brief 析构函数
     */
    ~SendNode()
    {
        delete[] _data;
    }
};

/**
 * @class CSession
 * @brief TCP 会话
 * @details 管理单个客户端连接的收发与业务处理。
 */
class CSession : public std::enable_shared_from_this<CSession>
{
public:
    /**
     * @brief 构造函数
     * @param ioc io_context 引用
     * @param server 所属服务器指针
     */
    CSession(boost::asio::io_context &ioc, CServer *server);
    /**
     * @brief 析构函数
     */
    ~CSession();
    /**
     * @brief 启动会话读循环
     */
    void Start();
    /**
     * @brief 关闭会话并释放资源
     */
    void Close();
    /**
     * @brief 发送消息
     * @param msg 消息体
     * @param msg_id 消息类型
     */
    void Send(const std::string &msg, short msg_id);
    /**
     * @brief 获取会话 UUID
     * @return std::string UUID
     */
    std::string GetUuid() const
    {
        return _uuid;
    }
    /**
     * @brief 获取底层 socket
     * @return boost::asio::ip::tcp::socket& socket 引用
     */
    boost::asio::ip::tcp::socket &GetSocket()
    {
        return _socket;
    }

private:
    void HandleLoginRequest(const std::string &body_data);
    void OnLoginValidated(int uid, bool valid);

    /**
     * @brief 异步读取消息头
     * @param total_len 头部长度
     */
    void AsyncReadHead(int total_len);
    /**
     * @brief 异步读取消息体
     * @param total_len 消息体长度
     */
    void AsyncReadBody(int total_len);
    /**
     * @brief 异步写出消息队列
     */
    void AsyncWriteMsg();
    /**
     * @brief 重置读超时时间
     */
    void ResetReadDeadline();

    boost::asio::ip::tcp::socket _socket;
    boost::asio::steady_timer _read_deadline;
    CServer *_server;
    std::string _uuid;                         ///< 会话 UUID
    std::shared_ptr<RecvNode> _recv_head_node; ///< 消息头缓冲
    std::shared_ptr<RecvNode> _recv_msg_node;  ///< 消息体缓冲

    // 发送队列相关
    std::queue<std::shared_ptr<SendNode>> _send_queue;
    std::mutex _send_mtx;
    bool _is_writing = false;

    // 用户UID
    int _user_uid = 0;                 ///< 已登录用户 UID
    std::atomic<bool> _login_in_progress{false};
    std::atomic<bool> _b_closed{false}; ///< 关闭状态
};

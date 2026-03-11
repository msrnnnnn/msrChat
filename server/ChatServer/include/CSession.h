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

class RecvNode
{
public:
    uint16_t _msg_id;
    uint32_t _total_len;
    char *_data;
    RecvNode(uint32_t max_len, uint16_t msg_id)
        : _total_len(max_len),
          _msg_id(msg_id)
    {
        _data = new char[_total_len + 1]();
    }
    ~RecvNode()
    {
        delete[] _data;
    }
    void Clear()
    {
        ::memset(_data, 0, _total_len + 1);
    }
};

class SendNode
{
public:
    uint16_t _msg_id;
    uint32_t _total_len;
    char *_data;
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
    ~SendNode()
    {
        delete[] _data;
    }
};

class CSession : public std::enable_shared_from_this<CSession>
{
public:
    CSession(boost::asio::io_context &ioc, CServer *server);
    ~CSession();
    void Start();
    void Close();
    void Send(const std::string &msg, short msg_id);
    std::string GetUuid() const
    {
        return _uuid;
    }
    boost::asio::ip::tcp::socket &GetSocket()
    {
        return _socket;
    }

private:
    void AsyncReadHead(int total_len);
    void AsyncReadBody(int total_len);
    void AsyncWriteMsg();
    void ResetReadDeadline();

    boost::asio::ip::tcp::socket _socket;
    boost::asio::steady_timer _read_deadline;
    CServer *_server;
    std::string _uuid;
    std::shared_ptr<RecvNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;

    // 发送队列相关
    std::queue<std::shared_ptr<SendNode>> _send_queue;
    std::mutex _send_mtx;
    bool _is_writing = false;

    // 用户UID
    int _user_uid = 0;
    std::atomic<bool> _b_closed{false};
};

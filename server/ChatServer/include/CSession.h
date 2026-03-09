#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

class CServer;

class RecvNode
{
public:
    short _msg_id;
    int _total_len;
    char *_data;
    RecvNode(short max_len, short msg_id)
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
    short _msg_id;
    int _total_len;
    char *_data;
    SendNode(const std::string &msg, short msg_id)
        : _msg_id(msg_id),
          _total_len(msg.length())
    {
        // 头部4字节 + 包体
        _data = new char[_total_len + 4]();
        // 写入消息ID (2字节，网络字节序)
        short net_msg_id = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
        memcpy(_data, &net_msg_id, 2);
        // 写入数据长度 (2字节，网络字节序)
        short net_len = boost::asio::detail::socket_ops::host_to_network_short(static_cast<short>(_total_len));
        memcpy(_data + 2, &net_len, 2);
        // 写入包体数据
        if (_total_len > 0)
        {
            memcpy(_data + 4, msg.data(), _total_len);
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

    boost::asio::ip::tcp::socket _socket;
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
};

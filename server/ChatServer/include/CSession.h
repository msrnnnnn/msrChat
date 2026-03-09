#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include <memory>
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

    boost::asio::ip::tcp::socket _socket;
    CServer *_server;
    std::string _uuid;
    std::shared_ptr<RecvNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;
};
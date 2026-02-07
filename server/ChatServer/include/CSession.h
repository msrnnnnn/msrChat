#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <queue>
#include <mutex>
#include "MsgNode.h"

class CServer;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
    CSession(boost::asio::io_context& io_context, CServer* server);
    ~CSession();
    boost::asio::ip::tcp::socket& GetSocket();
    std::string GetUuid();
    void Start();
    void Send(const std::string& msg, short msg_id);
    void Send(char* msg, short max_len, short msg_id);
    void Close();

private:
    void HandleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> self);
    void AsyncReadHead(int total_len);
    void AsyncReadBody(int total_len);
    void asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler);
    void asyncReadLen(std::size_t read_len, std::size_t total_len, std::function<void(const boost::system::error_code&, std::size_t)> handler);
    void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> self);

    boost::asio::ip::tcp::socket _socket;
    std::string _uuid;
    CServer* _server;
    char _data[MAX_LENGTH]; 
    std::shared_ptr<RecvNode> _recv_head_node;
    std::shared_ptr<RecvNode> _recv_msg_node;
    
    std::queue<std::shared_ptr<MsgNode>> _send_que;
    std::mutex _send_lock;
    bool _b_close;
};

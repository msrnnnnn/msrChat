#include "CSession.h"
#include "CServer.h"
#include "LogicSystem.h"
#include <cstring>
#include <boost/asio/detail/socket_ops.hpp> // For network_to_host_short
#include <spdlog/spdlog.h>

using namespace std;

CSession::CSession(boost::asio::io_context& io_context, CServer* server)
    : _socket(io_context), _server(server), _b_close(false)
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    _uuid = boost::uuids::to_string(uuid);
    _recv_head_node = make_shared<RecvNode>(HEAD_TOTAL_LEN, 0);
}

CSession::~CSession() {
    spdlog::info("~CSession destruct");
}

boost::asio::ip::tcp::socket& CSession::GetSocket()
{
    return _socket;
}

std::string CSession::GetUuid()
{
    return _uuid;
}

void CSession::Start()
{
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::Send(const std::string& msg, short msg_id) {
    std::lock_guard<std::mutex> lock(_send_lock);
    int send_que_size = _send_que.size();
    if (send_que_size > MAX_LENGTH) {
        spdlog::warn("session: {} send que fulled, size is {}", _uuid, MAX_LENGTH);
        return;
    }

    _send_que.push(make_shared<MsgNode>(msg.c_str(), msg.length(), msg_id));
    if (send_que_size > 0) {
        return;
    }
    
    auto& msgnode = _send_que.front();
    // 构造包头
    short msg_len = msgnode->_total_len;
    short msg_id_host = msgnode->_msg_id;
    // 转为网络字节序
    short msg_len_net = boost::asio::detail::socket_ops::host_to_network_short(msg_len);
    short msg_id_net = boost::asio::detail::socket_ops::host_to_network_short(msg_id_host);
    
    char* data = new char[HEAD_TOTAL_LEN + msg_len];
    memcpy(data, &msg_id_net, HEAD_ID_LEN);
    memcpy(data + HEAD_ID_LEN, &msg_len_net, HEAD_DATA_LEN);
    memcpy(data + HEAD_TOTAL_LEN, msgnode->_data, msg_len);
    
    auto self = shared_from_this();
    boost::asio::async_write(_socket, boost::asio::buffer(data, HEAD_TOTAL_LEN + msg_len),
        [this, self, data](const boost::system::error_code& error, std::size_t bytes_transferred) {
            delete[] data;
            HandleWrite(error, self);
        });
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> self) {
    if (error) {
        spdlog::error("HandleWrite failed, error is {}", error.message());
        Close();
        _server->ClearSession(_uuid);
        return;
    }
    
    std::lock_guard<std::mutex> lock(_send_lock);
    _send_que.pop();
    if (!_send_que.empty()) {
        auto& msgnode = _send_que.front();
        short msg_len = msgnode->_total_len;
        short msg_id_host = msgnode->_msg_id;
        short msg_len_net = boost::asio::detail::socket_ops::host_to_network_short(msg_len);
        short msg_id_net = boost::asio::detail::socket_ops::host_to_network_short(msg_id_host);
        
        char* data = new char[HEAD_TOTAL_LEN + msg_len];
        memcpy(data, &msg_id_net, HEAD_ID_LEN);
        memcpy(data + HEAD_ID_LEN, &msg_len_net, HEAD_DATA_LEN);
        memcpy(data + HEAD_TOTAL_LEN, msgnode->_data, msg_len);
        
        boost::asio::async_write(_socket, boost::asio::buffer(data, HEAD_TOTAL_LEN + msg_len),
            [this, self, data](const boost::system::error_code& error, std::size_t bytes_transferred) {
                delete[] data;
                HandleWrite(error, self);
            });
    }
}

void CSession::Close() {
    _socket.close();
    _b_close = true;
}

void CSession::AsyncReadHead(int total_len)
{
    auto self = shared_from_this();
    asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {
        try {
            if (ec) {
                spdlog::error("handle read failed, error is {}", ec.message());
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            if (bytes_transfered < HEAD_TOTAL_LEN) {
                spdlog::warn("read length not match, read [{}] , total [{}]", bytes_transfered, HEAD_TOTAL_LEN);
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            _recv_head_node->Clear();
            memcpy(_recv_head_node->_data, _data, bytes_transfered);

            //获取头部MSGID数据
            short msg_id = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            //网络字节序转化为本地字节序
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            spdlog::info("msg_id is {}", msg_id);
            //id非法
            if (msg_id > MAX_LENGTH) {
                spdlog::warn("invalid msg_id is {}", msg_id);
                _server->ClearSession(_uuid);
                return;
            }
            short msg_len = 0;
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
            //网络字节序转化为本地字节序
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
            spdlog::info("msg_len is {}", msg_len);

            //id非法
            if (msg_len > MAX_LENGTH) {
                spdlog::warn("invalid data length is {}", msg_len);
                _server->ClearSession(_uuid);
                return;
            }

            _recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);
            AsyncReadBody(msg_len);
        }
        catch (std::exception& e) {
            spdlog::error("Exception in AsyncReadHead: {}", e.what());
        }
    });
}

void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    asyncReadFull(total_len, [self, this, total_len](const boost::system::error_code& ec, std::size_t bytes_transfered) {
        try {
            if (ec) {
                spdlog::error("handle read failed, error is {}", ec.message());
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            if (bytes_transfered < total_len) {
                spdlog::warn("read length not match, read [{}] , total [{}]", bytes_transfered, total_len);
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            memcpy(_recv_msg_node->_data, _data, bytes_transfered);
            _recv_msg_node->_cur_len += bytes_transfered;
            _recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
            spdlog::info("receive data is {}", _recv_msg_node->_data);
            //此处将消息投递到逻辑队列中
            LogicSystem::GetInstance()->PostMsgToQue(make_shared<LogicNode>(shared_from_this(), _recv_msg_node));
            //继续监听头部接受事件
            AsyncReadHead(HEAD_TOTAL_LEN);
        }
        catch (std::exception& e) {
            spdlog::error("Exception in AsyncReadBody: {}", e.what());
        }
    });
}

void CSession::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
    ::memset(_data, 0, MAX_LENGTH);
    asyncReadLen(0, maxLength, handler);
}

void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len,
    std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
    auto self = shared_from_this();
    _socket.async_read_some(boost::asio::buffer(_data + read_len, total_len - read_len),
        [read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t  bytesTransfered) {
            if (ec) {
                // 出现错误，调用回调函数
                handler(ec, read_len + bytesTransfered);
                return;
            }

            if (read_len + bytesTransfered >= total_len) {
                //长度够了就调用回调函数
                handler(ec, read_len + bytesTransfered);
                return;
            }

            // 没有错误，且长度不足则继续读取
            self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
        });
}

void CSession::HandleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> self)
{
    // Not used in new logic, but kept for compatibility or reference
}

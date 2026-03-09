#include "CSession.h"
#include "CServer.h"
#include "const.h"

CSession::CSession(boost::asio::io_context &ioc, CServer *server)
    : _socket(ioc),
      _server(server)
{
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    _uuid = boost::uuids::to_string(a_uuid);
    _recv_head_node = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, 0);
}

CSession::~CSession()
{
    std::cout << "~CSession: " << _uuid << std::endl;
}
void CSession::Close()
{
    _socket.close();
}
void CSession::Start()
{
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::AsyncReadHead(int total_len)
{
    auto self = shared_from_this();
    boost::asio::async_read(
        _socket, boost::asio::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
        [this, self](const boost::system::error_code &ec, std::size_t bytes)
        {
            if (ec)
            {
                Close();
                _server->ClearSession(_uuid);
                return;
            }
            short msg_id = 0, msg_len = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);

            if (msg_len > MAX_LENGTH)
            {
                _server->ClearSession(_uuid);
                return;
            }
            _recv_msg_node = std::make_shared<RecvNode>(msg_len, msg_id);
            AsyncReadBody(msg_len);
        });
}

void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    boost::asio::async_read(
        _socket, boost::asio::buffer(_recv_msg_node->_data, total_len),
        [this, self, total_len](const boost::system::error_code &ec, std::size_t bytes)
        {
            if (ec)
            {
                Close();
                _server->ClearSession(_uuid);
                return;
            }
            _recv_msg_node->_data[total_len] = '\0';
            std::cout << "[Recv] ID: " << _recv_msg_node->_msg_id << " Data: " << _recv_msg_node->_data << std::endl;

            // 魔改精简版：收到什么直接发回去 (Echo)，验证全链路打通
            Send(std::string(_recv_msg_node->_data), _recv_msg_node->_msg_id);

            AsyncReadHead(HEAD_TOTAL_LEN);
        });
}

void CSession::Send(const std::string &msg, short msg_id)
{
    // 这里为了快速跑通，暂略异步发送队列，直接打印模拟发送
    std::cout << "[Send Echo] ID: " << msg_id << " Data: " << msg << std::endl;
}
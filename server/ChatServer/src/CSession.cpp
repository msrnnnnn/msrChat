#include "CSession.h"
#include "CServer.h"
#include "const.h"
#include <nlohmann/json.hpp>
#include <string>

CSession::CSession(boost::asio::io_context &ioc, CServer *server)
    : _socket(ioc),
      _server(server)
{
    _uuid = std::to_string(CServer::s_session_id_allocator.fetch_add(1));
    _recv_head_node = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, 0);
}

CSession::~CSession()
{
    std::cout << "~CSession: " << _uuid << std::endl;
}

void CSession::Close()
{
    // 如果已登录，从用户会话映射中移除
    if (_user_uid != 0)
    {
        _server->RemoveUserSession(_user_uid);
        _user_uid = 0;
    }
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
                // 如果已登录，从用户会话映射中移除
                if (_user_uid != 0)
                {
                    _server->RemoveUserSession(_user_uid);
                    _user_uid = 0;
                }
                Close();
                _server->ClearSession(_uuid);
                return;
            }
            short msg_id = 0, msg_len = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);

            if (msg_len == 0)
            {
                Close();
                _server->ClearSession(_uuid);
                return;
            }

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
                // 如果已登录，从用户会话映射中移除
                if (_user_uid != 0)
                {
                    _server->RemoveUserSession(_user_uid);
                    _user_uid = 0;
                }
                Close();
                _server->ClearSession(_uuid);
                return;
            }
            _recv_msg_node->_data[total_len] = '\0';
            std::cout << "[Recv] ID: " << _recv_msg_node->_msg_id << " Data: " << _recv_msg_node->_data << std::endl;

            // 处理消息
            short msg_id = _recv_msg_node->_msg_id;
            std::string body_data(_recv_msg_node->_data, total_len);

            try
            {
                if (msg_id == MSG_CHAT_LOGIN)
                {
                    // 解析登录 JSON
                    auto json_data = nlohmann::json::parse(body_data);
                    int uid = json_data.value("uid", 0);
                    std::string token = json_data.value("token", "");

                    std::cout << "[CSession] Login request - uid: " << uid << ", token: " << token << std::endl;

                    // 添加用户会话映射
                    _server->AddUserSession(uid, shared_from_this());
                    _user_uid = uid;

                    // 组装成功回复
                    nlohmann::json response;
                    response["code"] = 0;
                    response["msg"] = "login success";
                    response["uid"] = uid;
                    std::string response_str = response.dump();

                    Send(response_str, MSG_CHAT_LOGIN);
                }
                else if (msg_id == MSG_CHAT_TEXT)
                {
                    // 解析聊天消息 JSON
                    auto json_data = nlohmann::json::parse(body_data);
                    int from_uid = json_data.value("from_uid", 0);
                    int to_uid = json_data.value("to_uid", 0);
                    std::string content = json_data.value("content", "");

                    std::cout << "[CSession] Chat message - from: " << from_uid << ", to: " << to_uid
                              << ", content: " << content << std::endl;

                    // 转发消息到目标用户
                    _server->ForwardMessage(to_uid, body_data);
                }
                else
                {
                    // 其他消息类型，原样回显 (Echo)
                    Send(body_data, msg_id);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[CSession] JSON parse error: " << e.what() << std::endl;
                // 解析失败，发送错误回复
                nlohmann::json error_response;
                error_response["code"] = -1;
                error_response["msg"] = std::string("parse error: ") + e.what();
                Send(error_response.dump(), msg_id);
            }

            AsyncReadHead(HEAD_TOTAL_LEN);
        });
}

void CSession::Send(const std::string &msg, short msg_id)
{
    auto send_node = std::make_shared<SendNode>(msg, msg_id);
    std::lock_guard<std::mutex> lock(_send_mtx);
    _send_queue.push(send_node);
    if (!_is_writing)
    {
        _is_writing = true;
        AsyncWriteMsg();
    }
}

void CSession::AsyncWriteMsg()
{
    // 在锁内获取队首元素拷贝
    std::shared_ptr<SendNode> send_node;
    {
        std::lock_guard<std::mutex> lock(_send_mtx);
        if (_send_queue.empty())
        {
            _is_writing = false;
            return;
        }
        send_node = _send_queue.front();
    }

    // 释放锁后执行异步写操作
    auto self = shared_from_this();
    boost::asio::async_write(
        _socket, boost::asio::buffer(send_node->_data, send_node->_total_len + 4),
        [this, self](const boost::system::error_code &ec, std::size_t bytes)
        {
            if (ec)
            {
                // 如果已登录，从用户会话映射中移除
                if (_user_uid != 0)
                {
                    _server->RemoveUserSession(_user_uid);
                    _user_uid = 0;
                }
                Close();
                _server->ClearSession(_uuid);
                return;
            }

            // 在锁内执行 pop 操作
            bool has_more = false;
            {
                std::lock_guard<std::mutex> lock(_send_mtx);
                _send_queue.pop();
                has_more = !_send_queue.empty();
            }

            // 如果队列不为空，在锁外递归调用 AsyncWriteMsg
            if (has_more)
            {
                AsyncWriteMsg();
            }
            else
            {
                std::lock_guard<std::mutex> lock(_send_mtx);
                _is_writing = false;
            }
        });
}

/**
 * @file CSession.cpp
 * @brief TCP 会话实现
 * @details 负责协议解析、登录鉴权、消息转发与离线消息处理。
 */
#include "CSession.h"
#include "CServer.h"
#include "FileTransfer.h"
#include "LogicSystem.h"
#include "Message.pb.h"
#include "MessageDispatcher.h"
#include "SessionManager.h"
#include "MessageTask.h"
#include "SQLiteMgr.h"
#include "const.h"
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief 构造函数
 * @param ioc Boost ASIO io_context 引用
 * @param server CServer 弱引用指针
 * @details 初始化 UUID、接收节点池、读超时定时器
 */
CSession::CSession(boost::asio::io_context &ioc, std::shared_ptr<CServer> server)
    : _socket(ioc),
      _read_deadline(ioc),
      _strand(boost::asio::make_strand(ioc)),
      _expiry_time(std::chrono::steady_clock::now() + kReadTimeout),
      _server(server)
{
    _uuid = std::to_string(CServer::s_session_id_allocator.fetch_add(1));
    _recv_head_node = RecvNodePool().Acquire();
    _recv_head_node->Reset(HEAD_TOTAL_LEN, 0);
    _recv_msg_node = RecvNodePool().Acquire();
}

CSession::~CSession()
{
    spdlog::info("~CSession: {}", _uuid);
}

/**
 * @brief 关闭会话
 * @details 原子操作防止重复关闭，移除用户映射、关闭 Socket
 */
void CSession::Close()
{
    bool expected = false;
    if (!_closed.compare_exchange_strong(expected, true))
    {
        return;
    }
    if (_user_uid != 0)
    {
        FileTransfer::Instance().RemoveTaskBySession(_user_uid);
        auto server = _server.lock();
        if (server)
        {
            SessionManager::Instance().RemoveSession(_user_uid);
        }
        _user_uid = 0;
    }
    boost::system::error_code ec;
    _read_deadline.cancel(ec);
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    _socket.close(ec);
}

/**
 * @brief 启动会话
 * @details 在 Strand 上重置读超时、调度首次异步读头
 */
void CSession::Start()
{
    auto self = shared_from_this();
    boost::asio::dispatch(
        _strand,
        [this, self]()
        {
            ResetReadDeadline();
            ScheduleReadDeadlineCheck();
            AsyncReadHead();
        });
}

void CSession::ResetReadDeadline()
{
    _expiry_time = std::chrono::steady_clock::now() + kReadTimeout;
}

void CSession::ScheduleReadDeadlineCheck()
{
    _read_deadline.expires_after(kReadCheckInterval);
    auto self = shared_from_this();
    _read_deadline.async_wait(
        boost::asio::bind_executor(
            _strand,
            [this, self](const boost::system::error_code &ec)
            {
                if (ec)
                {
                    return;
                }

                if (_closed.load())
                {
                    return;
                }

                const auto now = std::chrono::steady_clock::now();
                if (now >= _expiry_time)
                {
                    spdlog::warn("[CSession] read timeout, closing session {}", _uuid);
                    TerminateSession("Read timeout");
                    return;
                }

                ScheduleReadDeadlineCheck();
            }));
}

/**
 * @brief 异步读取消息头（6 字节）
 * @details 解析 msg_id 和 msg_len，根据长度调度读 Body 或二进制 Body
 */
void CSession::AsyncReadHead()
{
    if (_closed.load())
    {
        _read_active.store(false);
        return;
    }
    auto self = shared_from_this();
    auto head_node = _recv_head_node;
    boost::asio::async_read(
        _socket, boost::asio::buffer(head_node->_data, HEAD_TOTAL_LEN),
        boost::asio::bind_executor(
            _strand,
            [this, self, head_node](const boost::system::error_code &ec, [[maybe_unused]] std::size_t bytes)
            {
                _read_active.store(false);

                if (ec)
                {
                    CleanupSession(ec);
                    return;
                }
                ResetReadDeadline();
                uint16_t msg_id = 0;
                uint32_t msg_len = 0;
                memcpy(&msg_id, head_node->_data, HEAD_ID_LEN);
                msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
                memcpy(&msg_len, head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
                msg_len = boost::asio::detail::socket_ops::network_to_host_long(msg_len);

                if (msg_len == 0)
                {
                    TerminateSession("Empty message length");
                    return;
                }

                if (msg_len > MAX_LENGTH)
                {
                    TerminateSession("Message length exceeds maximum");
                    return;
                }
                _recv_msg_node->Reset(msg_len, msg_id);
                AsyncReadBody(static_cast<int>(msg_len));
            }));
}

/**
 * @brief 异步读取消息体
 * @param total_len 消息体长度
 * @details 读取完成后投递到 LogicSystem 处理
 */
void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    auto recv_msg_node = _recv_msg_node;
    boost::asio::async_read(
        _socket, boost::asio::buffer(recv_msg_node->_data, total_len),
        boost::asio::bind_executor(
            _strand,
            [this, self, recv_msg_node,
             total_len](const boost::system::error_code &ec, [[maybe_unused]] std::size_t bytes)
            {
                if (ec)
                {
                    CleanupSession(ec);
                    return;
                }
                ResetReadDeadline();
                recv_msg_node->_data[total_len] = '\0';

                uint16_t msg_id = recv_msg_node->_msg_id;
                std::string body_data(recv_msg_node->_data, total_len);

                spdlog::debug("[CSession] Received msg_id {}, body_len={}, pushing to LogicSystem", msg_id, total_len);

                MessageTask task(shared_from_this(), msg_id, std::move(body_data));
                LogicSystem::getInstance().PostTask(std::move(task));
            }));
}

/**
 * @brief 登录验证结果处理
 * @param uid 用户 ID
 * @param valid Token 是否有效
 * @details 有效则注册会话到 CServer 并发送离线消息
 */
void CSession::OnLoginValidated(int uid, bool valid)
{
    _login_in_progress.store(false);

    if (_closed.load())
    {
        return;
    }

    nlohmann::json response;
    if (!valid)
    {
        spdlog::warn("[CSession] Token invalid for uid {}", uid);
        response["error"] = 1;
        response["message"] = "token invalid";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        ContinueReading();
        return;
    }

    if (_user_uid != 0)
    {
        response["error"] = 1;
        response["message"] = "already login";
        response["uid"] = _user_uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        ContinueReading();
        return;
    }

    auto server = _server.lock();
    if (server)
    {
        SessionManager::Instance().AddSession(uid, shared_from_this());
        _user_uid = uid;

        response["error"] = 0;
        response["message"] = "login success";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        server->SendOfflineMessages(uid, shared_from_this());
    }
    else
    {
        response["error"] = 1;
        response["message"] = "server shutting down";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
    }
    ContinueReading();
}

/**
 * @brief 发送消息（线程安全）
 * @param msg 消息内容
 * @param msg_id 消息类型 ID
 * @details 使用 Strand 保证发送顺序，队列满时自动抑制
 */
void CSession::Send(const std::string &msg, short msg_id)
{
    auto send_node = SendNodePool().Acquire();
    send_node->Reset(msg, static_cast<uint16_t>(msg_id));
    auto self = shared_from_this();
    boost::asio::dispatch(
        _strand,
        [this, self, send_node]()
        {
            if (_closed.load())
            {
                return;
            }
            _send_queue.push_back(send_node);
            if (_is_writing)
            {
                return;
            }
            _is_writing = true;
            AsyncWriteMsg();
        });
}

void CSession::AsyncWriteMsg()
{
    if (_send_queue.empty())
    {
        _is_writing = false;
        return;
    }
    auto send_node = _send_queue.front();
    auto self = shared_from_this();
    boost::asio::async_write(
        _socket, boost::asio::buffer(send_node->_data, send_node->_total_len + 6),
        boost::asio::bind_executor(
            _strand,
            [this, self, send_node](const boost::system::error_code &ec, [[maybe_unused]] std::size_t bytes)
            {
                if (ec)
                {
                    CleanupSession(ec);
                    return;
                }

                _send_queue.pop_front();
                if (_send_queue.empty())
                {
                    _is_writing = false;
                    return;
                }
                AsyncWriteMsg();
            }));
}

/**
 * @brief 分页发送离线消息
 * @details 每次发送 OFFLINE_PAGE_SIZE 条，发完一页后等待客户端 ACK 再继续
 */
void CSession::SendNextOfflinePage()
{
    std::lock_guard<std::recursive_mutex> lock(_offline_mutex);
    if (_offline_send_state.uid <= 0 || !_offline_send_state.sending)
    {
        return;
    }

    auto messages = SQLiteMgr::Instance().GetOfflineMessages(
        _offline_send_state.uid, OFFLINE_PAGE_SIZE,
        _offline_send_state.last_sent_id);

    if (messages.empty())
    {
        SQLiteMgr::Instance().ClearOfflineMessages(_offline_send_state.uid);
        _offline_send_state.sending = false;
        return;
    }

    for (const auto &msg : messages)
    {
        if (msg.type == 1)
        {
            // 图片离线消息：content 存的是序列化后的 ImageMsg protobuf binary
            Send(msg.content, MSG_CHAT_IMAGE);
            spdlog::debug("[CSession] SendNextOfflinePage: sent image msg id={} image_id={}",
                          msg.id, msg.image_id);
        }
        else
        {
            // 文本离线消息：保持原有逻辑
            qmsrchat::ServerChatMsg chatMsg;
            chatMsg.set_from_uid(msg.from_uid);
            chatMsg.set_to_uid(msg.to_uid);
            chatMsg.set_content(msg.content);
            if (!msg.client_msg_id.empty())
            {
                chatMsg.set_client_msg_id(msg.client_msg_id);
            }
            chatMsg.set_server_msg_id(msg.id);
            chatMsg.set_timestamp(msg.timestamp);

            std::string serialized;
            if (chatMsg.SerializeToString(&serialized))
            {
                Send(serialized, MSG_CHAT_TEXT);
            }
        }
    }

    _offline_send_state.sent_count += messages.size();
    _offline_send_state.last_sent_id = messages.back().id;

    nlohmann::json ack;
    ack["received"] = _offline_send_state.sent_count;
    ack["total"] = _offline_send_state.total_count;
    Send(ack.dump(), MSG_OFFLINE_ACK);

    if (_offline_send_state.sent_count >= _offline_send_state.total_count)
    {
        SQLiteMgr::Instance().ClearOfflineMessages(_offline_send_state.uid);
        _offline_send_state.sending = false;
    }
}

/**
 * @brief 继续离线消息发送
 * @details 收到离线 ACK 后检查是否还有未发完的消息
 */
void CSession::ContinueOfflineSend()
{
    std::lock_guard<std::recursive_mutex> lock(_offline_mutex);
    if (HasOfflineMessagesToSend())
    {
        SendNextOfflinePage();
    }
}

void CSession::CleanupSession(const boost::system::error_code &ec)
{
    if (ec)
    {
        if (ec == boost::asio::error::eof)
        {
            spdlog::info("[CSession] {}: client disconnected", _uuid);
        }
        else
        {
            spdlog::error("[CSession] {}: {}", _uuid, ec.message());
        }
    }
    if (_user_uid != 0)
    {
        auto server = _server.lock();
        if (server)
        {
            SessionManager::Instance().RemoveSession(_user_uid);
        }
        _user_uid = 0;
    }
    Close();
    auto server = _server.lock();
    if (server)
    {
        SessionManager::Instance().RemoveSessionByUuid(_uuid);
    }
}

void CSession::TerminateSession(const std::string &error_msg)
{
    if (!error_msg.empty())
    {
        spdlog::error("[CSession] {}: {}", _uuid, error_msg);
    }
    Close();
    auto server = _server.lock();
    if (server)
    {
        SessionManager::Instance().RemoveSessionByUuid(_uuid);
    }
}

/**
 * @file CSession.cpp
 * @brief TCP 会话实现
 * @details 负责协议解析、登录鉴权、消息转发与离线消息处理。
 */
#include "CSession.h"
#include "CServer.h"
#include "LogicSystem.h"
#include "MessageDispatcher.h"
#include "MessageTask.h"
#include "SQLiteMgr.h"
#include "const.h"
#include "Message.pb.h"
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

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
    _recv_bin_head_node = RecvNodePool().Acquire();
    _recv_bin_head_node->Reset(HEAD_BIN_TOTAL_LEN, 0);
}

CSession::~CSession()
{
    spdlog::info("~CSession: {}", _uuid);
}

void CSession::Close()
{
    bool expected = false;
    if (!_b_closed.compare_exchange_strong(expected, true))
    {
        return;
    }
    if (_user_uid != 0)
    {
        auto server = _server.lock();
        if (server)
        {
            server->RemoveUserSession(_user_uid);
        }
        _user_uid = 0;
    }
    boost::system::error_code ec;
    _read_deadline.cancel(ec);
    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    _socket.close(ec);
}

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

                if (_b_closed.load())
                {
                    return;
                }

                const auto now = std::chrono::steady_clock::now();
                if (now >= _expiry_time)
                {
                    spdlog::warn("[CSession] read timeout, closing session {}", _uuid);
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
                    return;
                }

                ScheduleReadDeadlineCheck();
            }));
}

void CSession::AsyncReadHead()
{
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
                    if (_user_uid != 0)
                    {
                        auto server = _server.lock();
                        if (server)
                        {
                            server->RemoveUserSession(_user_uid);
                        }
                        _user_uid = 0;
                    }
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
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
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
                    return;
                }

                if (IsBinaryPacket(msg_id))
                {
                    if (msg_len > HEAD_BIN_TOTAL_LEN)
                    {
                        _bin_packet_state.msg_id = msg_id;
                        _bin_packet_state.total_len = msg_len;
                        _bin_packet_state.receiving = true;
                        _recv_msg_node->Reset(msg_len, msg_id);
                        AsyncReadBinBody(msg_len);
                    }
                    else
                    {
                        auto server = _server.lock();
                        if (server)
                        {
                            server->ClearSession(_uuid);
                        }
                    }
                    return;
                }

                if (msg_len > MAX_LENGTH)
                {
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
                    return;
                }
                _recv_msg_node->Reset(msg_len, msg_id);
                AsyncReadBody(static_cast<int>(msg_len));
            }));
}

void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    auto recv_msg_node = _recv_msg_node;
    boost::asio::async_read(
        _socket, boost::asio::buffer(recv_msg_node->_data, total_len),
        boost::asio::bind_executor(
            _strand,
            [this, self, recv_msg_node, total_len](const boost::system::error_code &ec, [[maybe_unused]] std::size_t bytes)
            {
                if (ec)
                {
                    if (_user_uid != 0)
                    {
                        auto server = _server.lock();
                        if (server)
                        {
                            server->RemoveUserSession(_user_uid);
                        }
                        _user_uid = 0;
                    }
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
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

void CSession::OnLoginValidated(int uid, bool valid)
{
    _login_in_progress.store(false);

    if (_b_closed.load())
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
        server->AddUserSession(uid, shared_from_this());
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

void CSession::Send(const std::string &msg, short msg_id)
{
    auto send_node = SendNodePool().Acquire();
    send_node->Reset(msg, static_cast<uint16_t>(msg_id));
    auto self = shared_from_this();
    boost::asio::dispatch(
        _strand,
        [this, self, send_node]()
        {
            _send_queue.push_back(send_node);
            if (_is_writing)
            {
                return;
            }
            _is_writing = true;
            AsyncWriteMsg();
        });
}

void CSession::SendBinary(const std::string &json_data, const std::vector<char> &binary_data, short msg_id)
{
    uint32_t json_len = json_data.size();
    uint32_t binary_len = binary_data.size();
    uint32_t total_len = json_len + binary_len;

    if (total_len > HEAD_BIN_MAX_LENGTH)
    {
        spdlog::error("[CSession] Binary packet too large: {}", total_len);
        return;
    }

    auto send_node = SendNodePool().Acquire();
    send_node->ResetBinary(msg_id, json_data, binary_data);

    auto self = shared_from_this();
    boost::asio::dispatch(
        _strand,
        [this, self, send_node]()
        {
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
                    if (_user_uid != 0)
                    {
                        auto server = _server.lock();
                        if (server)
                        {
                            server->RemoveUserSession(_user_uid);
                        }
                        _user_uid = 0;
                    }
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
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

void CSession::StartFileSend(int64_t task_id, const std::string &filepath)
{
    std::lock_guard<std::mutex> lock(_file_mutex);
    if (_file_send_state.sending)
        return;
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0)
        return;
    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return;
    }
    _file_send_state.task_id = task_id;
    _file_send_state.fd.Reset(fd);
    _file_send_state.total_size = st.st_size;
    _file_send_state.sent_size = 0;
    _file_send_state.filename = filepath;
    _file_send_state.sending = true;
    spdlog::info("[CSession] Start file send: task={}, file={}, size={}", task_id, filepath, st.st_size);
}

void CSession::SendNextOfflinePage()
{
    if (_offline_send_state.uid <= 0 || !_offline_send_state.sending)
    {
        return;
    }

    auto messages = SQLiteMgr::Instance().GetOfflineMessages(
        _offline_send_state.uid, OFFLINE_PAGE_SIZE);

    if (messages.empty())
    {
        SQLiteMgr::Instance().ClearOfflineMessages(_offline_send_state.uid);
        _offline_send_state.sending = false;
        return;
    }

    for (const auto &msg : messages)
    {
        qmsrchat::ServerChatMsg chatMsg;
        chatMsg.set_from_uid(msg.from_uid);
        chatMsg.set_to_uid(msg.to_uid);
        chatMsg.set_content(msg.content);
        chatMsg.set_server_msg_id(msg.id);
        chatMsg.set_timestamp(msg.timestamp);

        std::string serialized;
        if (chatMsg.SerializeToString(&serialized))
        {
            Send(serialized, MSG_CHAT_TEXT);
        }
    }

    _offline_send_state.sent_count += messages.size();

    if (_offline_send_state.sent_count >= _offline_send_state.total_count)
    {
        SQLiteMgr::Instance().ClearOfflineMessages(_offline_send_state.uid);
        _offline_send_state.sending = false;
    }
}

void CSession::SendNextFileChunk()
{
    std::lock_guard<std::mutex> lock(_file_mutex);

    if (!_file_send_state.sending)
    {
        return;
    }

    int64_t remain = _file_send_state.total_size - _file_send_state.sent_size;
    if (remain <= 0)
    {
        _file_send_state.sending = false;
        return;
    }

    char buffer[CHUNK_SIZE];
    int64_t to_read = std::min(static_cast<int64_t>(CHUNK_SIZE), remain);
    ssize_t bytes_read = read(_file_send_state.fd, buffer, to_read);

    if (bytes_read <= 0)
    {
        _file_send_state.sending = false;
        _file_send_state.fd.Reset();
        return;
    }

    std::vector<char> chunk_data(buffer, buffer + bytes_read);
    nlohmann::json json_meta;
    json_meta["task_id"] = _file_send_state.task_id;
    json_meta["offset"] = _file_send_state.sent_size;
    json_meta["size"] = bytes_read;

    std::string json_str = json_meta.dump();
    SendBinary(json_str, chunk_data, MSG_FILE_CHUNK);

    _file_send_state.sent_size += bytes_read;
}

void CSession::AsyncReadBinBody(int total_len)
{
    auto self = shared_from_this();
    auto recv_msg_node = _recv_msg_node;
    boost::asio::async_read(
        _socket, boost::asio::buffer(recv_msg_node->_data, total_len),
        boost::asio::bind_executor(
            _strand,
            [this, self, recv_msg_node, total_len](const boost::system::error_code &ec, [[maybe_unused]] std::size_t bytes)
            {
                if (ec)
                {
                    if (_user_uid != 0)
                    {
                        auto server = _server.lock();
                        if (server)
                        {
                            server->RemoveUserSession(_user_uid);
                        }
                        _user_uid = 0;
                    }
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
                    return;
                }

                _bin_packet_state.receiving = false;
                spdlog::info(
                    "[CSession] Binary packet received: task_id={}, total_len={}", _bin_packet_state.msg_id, total_len);

                nlohmann::json response{{"error", 0}, {"msg_id", _bin_packet_state.msg_id}};
                Send(response.dump(), MSG_CHAT_ACK);

                AsyncReadHead();
            }));
}

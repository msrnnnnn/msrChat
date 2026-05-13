/**
 * @file CSession.cpp
 * @brief TCP 会话实现
 * @details 负责协议解析、登录鉴权、消息转发与离线消息处理。
 */
#include "CSession.h"
#include "CServer.h"
#include "LogicSystem.h"
#include "Message.pb.h"
#include "MessageDispatcher.h"
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

namespace
{
bool BuildFileChunkPayload(
    std::string_view json_meta_view, std::string_view binary_payload_view, std::string &out_payload)
{
    const auto json_meta = nlohmann::json::parse(std::string(json_meta_view), nullptr, false);
    if (json_meta.is_discarded())
    {
        spdlog::warn("[CSession] Invalid binary chunk json metadata");
        return false;
    }

    const auto task_id = json_meta.value("task_id", static_cast<int64_t>(0));
    const auto offset = json_meta.value("offset", static_cast<int64_t>(0));
    const auto declared_size = json_meta.value("size", static_cast<int64_t>(binary_payload_view.size()));

    if (task_id <= 0 || offset < 0 || declared_size < 0)
    {
        spdlog::warn(
            "[CSession] Invalid binary chunk metadata: task_id={}, offset={}, size={}", task_id, offset, declared_size);
        return false;
    }

    if (static_cast<std::size_t>(declared_size) != binary_payload_view.size())
    {
        spdlog::warn(
            "[CSession] Binary chunk size mismatch: declared={}, actual={}", declared_size, binary_payload_view.size());
    }

    qmsrchat::FileChunk chunk;
    chunk.set_task_id(task_id);
    chunk.set_offset(offset);
    chunk.set_size(static_cast<int64_t>(binary_payload_view.size()));
    chunk.set_data(binary_payload_view.data(), static_cast<int>(binary_payload_view.size()));

    return chunk.SerializeToString(&out_payload);
}

bool DecodeBinaryPayload(uint16_t msg_id, const char *data, int total_len, std::string &out_payload)
{
    if (total_len < HEAD_BIN_JSON_LEN_FIELD)
    {
        spdlog::warn("[CSession] Binary payload too short: {}", total_len);
        return false;
    }

    uint32_t json_len = 0;
    std::memcpy(&json_len, data, HEAD_BIN_JSON_LEN_FIELD);
    json_len = boost::asio::detail::socket_ops::network_to_host_long(json_len);

    const auto packet_body_len = static_cast<uint32_t>(total_len - HEAD_BIN_JSON_LEN_FIELD);
    if (json_len > packet_body_len)
    {
        spdlog::warn("[CSession] Invalid binary packet lengths: json_len={}, body_len={}", json_len, packet_body_len);
        return false;
    }

    const std::string_view json_meta_view(data + HEAD_BIN_JSON_LEN_FIELD, json_len);
    const std::string_view binary_payload_view(data + HEAD_BIN_JSON_LEN_FIELD + json_len, packet_body_len - json_len);

    if (msg_id == MSG_FILE_CHUNK)
    {
        if (!BuildFileChunkPayload(json_meta_view, binary_payload_view, out_payload))
        {
            spdlog::warn("[CSession] Failed to convert binary file chunk to dispatcher payload");
            return false;
        }
        return true;
    }

    out_payload.assign(data, total_len);
    return true;
}
} // namespace

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
    _recv_bin_head_node = RecvNodePool().Acquire();
    _recv_bin_head_node->Reset(HEAD_BIN_TOTAL_LEN, 0);
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

/**
 * @brief 异步读取消息头（6 字节）
 * @details 解析 msg_id 和 msg_len，根据长度调度读 Body 或二进制 Body
 */
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
                    if (msg_len >= HEAD_BIN_JSON_LEN_FIELD && msg_len <= HEAD_BIN_MAX_LENGTH)
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

/**
 * @brief 登录验证结果处理
 * @param uid 用户 ID
 * @param valid Token 是否有效
 * @details 有效则注册会话到 CServer 并发送离线消息
 */
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
            _send_queue.push_back(send_node);
            if (_is_writing)
            {
                return;
            }
            _is_writing = true;
            AsyncWriteMsg();
        });
}

/**
 * @brief 发送二进制消息（包含 JSON 元数据和二进制负载）
 * @param json_data JSON 元数据
 * @param binary_data 二进制负载数据
 * @param msg_id 消息类型 ID
 */
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

/**
 * @brief 启动文件发送
 * @param task_id 任务 ID
 * @param filepath 文件路径
 * @details 以只读模式打开文件，记录文件描述符和大小
 */
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

/**
 * @brief 分页发送离线消息
 * @details 每次发送 OFFLINE_PAGE_SIZE 条，发完一页后等待客户端 ACK 再继续
 */
void CSession::SendNextOfflinePage()
{
    if (_offline_send_state.uid <= 0 || !_offline_send_state.sending)
    {
        return;
    }

    auto messages = SQLiteMgr::Instance().GetOfflineMessages(_offline_send_state.uid, OFFLINE_PAGE_SIZE);

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
    if (HasOfflineMessagesToSend())
    {
        SendNextOfflinePage();
    }
}

/**
 * @brief 发送下一个文件分片
 * @details 每次发送 CHUNK_SIZE 大小的块，更新发送进度
 */
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
        _file_send_state.fd.Reset();
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

    qmsrchat::FileChunk chunk;
    chunk.set_task_id(_file_send_state.task_id);
    chunk.set_offset(_file_send_state.sent_size);
    chunk.set_size(bytes_read);
    chunk.set_data(buffer, bytes_read);

    std::string serialized;
    if (chunk.SerializeToString(&serialized))
    {
        Send(serialized, MSG_FILE_CHUNK);
    }

    _file_send_state.sent_size += bytes_read;

    if (_file_send_state.sent_size >= _file_send_state.total_size)
    {
        _file_send_state.sending = false;
        _file_send_state.fd.Reset();
    }
}

void CSession::AsyncReadBinBody(int total_len)
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
                ResetReadDeadline();

                std::string body_data;
                if (!DecodeBinaryPayload(_bin_packet_state.msg_id, recv_msg_node->_data, total_len, body_data))
                {
                    Close();
                    auto server = _server.lock();
                    if (server)
                    {
                        server->ClearSession(_uuid);
                    }
                    return;
                }

                spdlog::debug(
                    "[CSession] Binary packet decoded: msg_id={}, total_len={}, body_len={}", _bin_packet_state.msg_id,
                    total_len, body_data.size());

                MessageTask task(shared_from_this(), _bin_packet_state.msg_id, std::move(body_data));
                LogicSystem::getInstance().PostTask(std::move(task));
            }));
}

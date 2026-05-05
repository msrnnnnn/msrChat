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
#include <sys/sendfile.h>
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

void CSession::HandleLoginRequest(const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data, nullptr, false);
    nlohmann::json response;

    if (json_data.is_discarded())
    {
        response["error"] = 1;
        response["message"] = "invalid login payload";
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead();
        return;
    }

    int uid = json_data.value("uid", 0);
    std::string token = json_data.value("token", "");

    if (uid <= 0 || token.empty())
    {
        response["error"] = 1;
        response["message"] = "invalid login";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead();
        return;
    }

    if (_user_uid != 0)
    {
        response["error"] = 1;
        response["message"] = "already login";
        response["uid"] = _user_uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead();
        return;
    }

    bool expected = false;
    if (!_login_in_progress.compare_exchange_strong(expected, true))
    {
        response["error"] = 1;
        response["message"] = "login in progress";
        response["uid"] = uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead();
        return;
    }

    auto server = _server.lock();
    bool token_valid = false;
    if (server)
    {
        token_valid = server->CheckToken(uid, token);
    }
    OnLoginValidated(uid, token_valid);
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
        AsyncReadHead();
        return;
    }

    if (_user_uid != 0)
    {
        response["error"] = 1;
        response["message"] = "already login";
        response["uid"] = _user_uid;
        Send(response.dump(), MSG_CHAT_LOGIN);
        AsyncReadHead();
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
    AsyncReadHead();
}

void CSession::HandleRegisterRequest(const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data);
    std::string username = json_data.value("user", "");
    std::string password_hash = json_data.value("passwd", "");
    std::string email = json_data.value("email", "");

    if (username.empty() || password_hash.empty() || email.empty())
    {
        nlohmann::json response{{"error", 1}};
        auto response_str = response.dump();
        boost::asio::post(
            _strand,
            [this, response_str]()
            {
                Send(response_str, ID_REGISTER_USER);
                AsyncReadHead();
            });
        return;
    }

    auto self = shared_from_this();
    auto server = _server.lock();
    if (!server)
    {
        return;
    }
    server->GetThreadPool().Enqueue(
        [this, self, server, username, password_hash, email]()
        {
            AuthResult result = SQLiteMgr::Instance().RegisterUser(username, password_hash, email);

            boost::asio::post(
                _strand,
                [this, self, result]()
                {
                    nlohmann::json response;
                    response["error"] = result.error;
                    if (result.error == 0)
                    {
                        response["uid"] = result.uid;
                        response["username"] = result.username;
                    }
                    Send(response.dump(), ID_REGISTER_USER);
                    AsyncReadHead();
                });
        });
}

void CSession::HandleLoginAuthRequest(const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data);
    std::string username = json_data.value("user", "");
    std::string password_hash = json_data.value("passwd", "");

    if (username.empty() || password_hash.empty())
    {
        nlohmann::json response;
        response["error"] = 1;
        response["message"] = "invalid parameters";
        auto response_str = response.dump();
        boost::asio::post(
            _strand,
            [this, response_str]()
            {
                Send(response_str, ID_LOGIN_USER);
                AsyncReadHead();
            });
        return;
    }

    auto self = shared_from_this();
    auto server = _server.lock();
    if (!server)
    {
        return;
    }
    server->GetThreadPool().Enqueue(
        [this, self, server, username, password_hash]()
        {
            AuthResult result = SQLiteMgr::Instance().LoginUser(username, password_hash);

            boost::asio::post(
                _strand,
                [this, self, result, server]()
                {
                    nlohmann::json response;
                    response["error"] = result.error;
                    if (result.error != 0)
                    {
                        Send(response.dump(), ID_LOGIN_USER);
                        AsyncReadHead();
                        return;
                    }

                    server->SetToken(result.uid, result.token);
                    response["uid"] = result.uid;
                    response["username"] = result.username;
                    response["token"] = result.token;
                    spdlog::info("[CSession] User {} auth login success, token issued", result.uid);

                    Send(response.dump(), ID_LOGIN_USER);
                    AsyncReadHead();
                });
        });
}

void CSession::HandleGetVerifyCodeRequest(const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data);
    std::string email = json_data.value("email", "");

    if (email.empty())
    {
        nlohmann::json response{{"error", 1}};
        auto response_str = response.dump();
        boost::asio::post(
            _strand,
            [this, response_str]()
            {
                Send(response_str, ID_GET_VARIFY_CODE);
                AsyncReadHead();
            });
        return;
    }

    auto self = shared_from_this();
    auto server = _server.lock();
    if (!server)
    {
        return;
    }
    server->GetThreadPool().Enqueue(
        [this, self, email]()
        {
            bool success = SQLiteMgr::Instance().SendVerifyCode(email);

            boost::asio::post(
                _strand,
                [this, self, success]()
                {
                    nlohmann::json response{{"error", success ? 0 : 1}};
                    Send(response.dump(), ID_GET_VARIFY_CODE);
                    AsyncReadHead();
                });
        });
}

void CSession::HandleResetPwdRequest(const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data);
    std::string username = json_data.value("user", "");
    std::string email = json_data.value("email", "");
    std::string code = json_data.value("varifycode", "");
    std::string new_password_hash = json_data.value("passwd", "");

    if (username.empty() || email.empty() || code.empty() || new_password_hash.empty())
    {
        nlohmann::json response{{"error", 1}};
        auto response_str = response.dump();
        boost::asio::post(
            _strand,
            [this, response_str]()
            {
                Send(response_str, ID_RESET_PWD);
                AsyncReadHead();
            });
        return;
    }

    auto self = shared_from_this();
    auto server = _server.lock();
    if (!server)
    {
        return;
    }
    server->GetThreadPool().Enqueue(
        [this, self, username, email, code, new_password_hash]()
        {
            int verify_result = SQLiteMgr::Instance().CheckVerifyCode(email, code);
            bool success = false;
            int error_code = verify_result;

            if (verify_result == 0)
            {
                success = SQLiteMgr::Instance().ResetPassword(username, email, code, new_password_hash);
                error_code = success ? 0 : 1009;
            }

            boost::asio::post(
                _strand,
                [this, self, error_code]()
                {
                    nlohmann::json response{{"error", error_code}};
                    Send(response.dump(), ID_RESET_PWD);
                    AsyncReadHead();
                });
        });
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

void CSession::HandleFileReq(const std::string &body_data)
{
    if (_user_uid == 0)
    {
        nlohmann::json response{{"error", 1}, {"message", "not login"}};
        Send(response.dump(), MSG_FILE_ACK);
        AsyncReadHead();
        return;
    }

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        int to_uid = json_data.value("to_uid", 0);
        std::string filename = json_data.value("filename", "");
        int64_t total_size = json_data.value("total_size", 0);

        if (task_id <= 0 || to_uid <= 0 || filename.empty() || total_size <= 0)
        {
            nlohmann::json response{{"error", 1}, {"message", "invalid file request"}};
            Send(response.dump(), MSG_FILE_ACK);
            AsyncReadHead();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(_file_mutex);
            _file_recv_state.task_id = task_id;
            _file_recv_state.from_uid = _user_uid;
            _file_recv_state.to_uid = to_uid;
            _file_recv_state.filename = filename;
            _file_recv_state.total_size = total_size;
            _file_recv_state.received_size = 0;
            _file_recv_state.data.clear();
            _file_recv_state.data.reserve(static_cast<size_t>(total_size));
            _file_recv_state.transfer_ready = true;
        }

        nlohmann::json response{{"error", 0}, {"task_id", task_id}, {"message", "ready to receive"}};
        Send(response.dump(), MSG_FILE_ACK);

        nlohmann::json forward;
        forward["task_id"] = task_id;
        forward["from_uid"] = _user_uid;
        forward["filename"] = filename;
        forward["total_size"] = total_size;
        auto server = _server.lock();
        if (server)
        {
            server->ForwardMessage(to_uid, forward.dump());
        }

        spdlog::info("[CSession] File transfer ready: task={}, file={}, size={}", task_id, filename, total_size);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleFileReq error: {}", e.what());
        nlohmann::json response{{"error", 1}, {"message", "parse error"}};
        Send(response.dump(), MSG_FILE_ACK);
    }

    AsyncReadHead();
}

void CSession::HandleFileChunk(const std::string &body_data)
{
    HandleFileChunk(std::string_view(body_data));
}

void CSession::HandleFileChunk(std::string_view body_view)
{
    if (_user_uid == 0)
    {
        nlohmann::json response{{"error", 1}, {"message", "not login"}};
        Send(response.dump(), MSG_FILE_ACK);
        AsyncReadHead();
        return;
    }

    try
    {
        nlohmann::json json_data;
        std::string_view chunk_view;

        auto json_start = body_view.find('{');
        if (json_start != std::string_view::npos)
        {
            auto json_end = body_view.find('}', json_start);
            if (json_end != std::string_view::npos)
            {
                std::string_view json_view = body_view.substr(json_start, json_end - json_start + 1);
                json_data = nlohmann::json::parse(json_view);

                size_t data_start = json_end + 1;
                if (data_start < body_view.size())
                {
                    chunk_view = body_view.substr(data_start);
                }
            }
        }

        if (json_data.empty())
        {
            json_data = nlohmann::json::parse(body_view);
            chunk_view = std::string_view();
        }

        int64_t task_id = json_data.value("task_id", 0);
        int64_t chunk_size = json_data.value("size", 0);

        std::lock_guard<std::mutex> lock(_file_mutex);

        if (!_file_recv_state.transfer_ready || _file_recv_state.task_id != task_id)
        {
            spdlog::warn("[CSession] File chunk received without proper setup, task_id={}", task_id);
            AsyncReadHead();
            return;
        }

        if (!chunk_view.empty())
        {
            AppendFileChunk(task_id, chunk_view);
        }
        else if (chunk_size > 0 && static_cast<int64_t>(body_view.size()) > chunk_size)
        {
            size_t actual_data_start = body_view.size() - static_cast<size_t>(chunk_size);
            std::string_view data_view = body_view.substr(actual_data_start);
            AppendFileChunk(task_id, data_view);
        }

        int progress = static_cast<int>((_file_recv_state.received_size * 100) / _file_recv_state.total_size);
        spdlog::debug(
            "[CSession] File chunk: task={}, received={}/{}, progress={}%", task_id, _file_recv_state.received_size,
            _file_recv_state.total_size, progress);

        if (_file_recv_state.received_size >= _file_recv_state.total_size)
        {
            spdlog::info("[CSession] File transfer completed: task={}, file={}", task_id, _file_recv_state.filename);

            nlohmann::json response{{"error", 0}, {"task_id", task_id}, {"message", "transfer complete"}};
            Send(response.dump(), MSG_FILE_ACK);

            _file_recv_state.transfer_ready = false;
            _file_recv_state.data.clear();
        }
        else
        {
            nlohmann::json response{{"error", 0}, {"task_id", task_id}, {"received", _file_recv_state.received_size}};
            Send(response.dump(), MSG_FILE_ACK);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleFileChunk error: {}", e.what());
        nlohmann::json response{{"error", 1}, {"message", "chunk processing error"}};
        Send(response.dump(), MSG_FILE_ACK);
    }

    AsyncReadHead();
}

void CSession::HandleFileAck(const std::string &body_data)
{
    if (_user_uid == 0)
    {
        AsyncReadHead();
        return;
    }

    bool need_send_next_chunk = false;
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        int error = json_data.value("error", 0);
        std::string message = json_data.value("message", "");

        spdlog::info("[CSession] File ACK: task={}, error={}, message={}", task_id, error, message);

        {
            std::lock_guard<std::mutex> lock(_file_mutex);
            if (_file_send_state.sending && _file_send_state.task_id == task_id)
            {
                if (error == 0)
                {
                    if (_file_send_state.sent_size >= _file_send_state.total_size)
                    {
                        _file_send_state.sending = false;
                        _file_send_state.fd.Reset();
                        spdlog::info(
                            "[CSession] File send completed: task={}, file={}", task_id, _file_send_state.filename);
                    }
                    else
                    {
                        need_send_next_chunk = true;
                    }
                }
                else
                {
                    _file_send_state.sending = false;
                    _file_send_state.fd.Reset();
                    spdlog::error("[CSession] File send failed: task={}, message={}", task_id, message);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleFileAck error: {}", e.what());
    }

    if (need_send_next_chunk)
    {
        SendNextFileChunk();
    }

    AsyncReadHead();
}

void CSession::HandleFileRsp(const std::string &body_data)
{
    if (_user_uid == 0)
    {
        AsyncReadHead();
        return;
    }

    bool need_send_next_chunk = false;
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        int error = json_data.value("error", 0);
        int64_t offset = json_data.value("offset", 0);

        spdlog::info("[CSession] File RSP: task={}, error={}, offset={}", task_id, error, offset);

        {
            std::lock_guard<std::mutex> lock(_file_mutex);
            if (error == 0 && _file_send_state.sending && _file_send_state.task_id == task_id)
            {
                _file_send_state.sent_size = offset;
                need_send_next_chunk = true;
            }
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleFileRsp error: {}", e.what());
    }

    if (need_send_next_chunk)
    {
        SendNextFileChunk();
    }

    AsyncReadHead();
}

void CSession::HandleOfflineAck(const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int count = json_data.value("count", 0);
        spdlog::debug("[CSession] Offline messages acknowledged: {}", count);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleOfflineAck error: {}", e.what());
    }

    AsyncReadHead();
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

void CSession::HandleZeroCopyStart(const std::string &body_data)
{
    if (_user_uid == 0)
    {
        nlohmann::json response{{"error", 1}, {"message", "not login"}};
        Send(response.dump(), MSG_ZEROCOPY_ERROR);
        AsyncReadHead();
        return;
    }

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        std::string filename = json_data.value("filename", "");
        int64_t total_size = json_data.value("total_size", 0);
        int to_uid = json_data.value("to_uid", 0);

        if (task_id <= 0 || filename.empty() || total_size <= 0)
        {
            nlohmann::json response{{"error", 1}, {"message", "invalid zero-copy request"}};
            Send(response.dump(), MSG_ZEROCOPY_ERROR);
            AsyncReadHead();
            return;
        }

        std::string save_path = "./uploads/" + filename;
        int fd = open(save_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            spdlog::error("[CSession] Failed to open file for zero-copy receive: {}", save_path);
            nlohmann::json response{{"error", 1}, {"message", "cannot open file"}};
            Send(response.dump(), MSG_ZEROCOPY_ERROR);
            AsyncReadHead();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(_file_mutex);
            _zc_recv_state.task_id = task_id;
            _zc_recv_state.fd.Reset(fd);
            _zc_recv_state.total_size = total_size;
            _zc_recv_state.received_size = 0;
            _zc_recv_state.filename = filename;
            _zc_recv_state.receiving = true;
        }

        nlohmann::json response{{"error", 0}, {"task_id", task_id}, {"message", "ready for zero-copy transfer"}};
        Send(response.dump(), MSG_ZEROCOPY_READY);

        nlohmann::json forward;
        forward["task_id"] = task_id;
        forward["from_uid"] = _user_uid;
        forward["filename"] = filename;
        forward["total_size"] = total_size;
        auto server = _server.lock();
        if (server)
        {
            server->ForwardMessage(to_uid, forward.dump());
        }

        spdlog::info("[CSession] Zero-copy receive ready: task={}, file={}, size={}", task_id, filename, total_size);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleZeroCopyStart error: {}", e.what());
        nlohmann::json response{{"error", 1}, {"message", "parse error"}};
        Send(response.dump(), MSG_ZEROCOPY_ERROR);
    }

    AsyncReadHead();
}

void CSession::HandleZeroCopyData(const std::string &body_data)
{
    if (_user_uid == 0 || !_zc_recv_state.receiving)
    {
        AsyncReadHead();
        return;
    }

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        int64_t size = json_data.value("size", 0);

        if (task_id != _zc_recv_state.task_id)
        {
            spdlog::warn("[CSession] Zero-copy data task_id mismatch");
            AsyncReadHead();
            return;
        }

        if (size <= 0)
        {
            std::lock_guard<std::mutex> lock(_file_mutex);
            _zc_recv_state.fd.Reset();
            _zc_recv_state.receiving = false;

            nlohmann::json response{{"error", 0}, {"task_id", task_id}};
            Send(response.dump(), MSG_ZEROCOPY_COMPLETE);

            spdlog::info(
                "[CSession] Zero-copy receive completed: task={}, size={}", task_id, _zc_recv_state.received_size);
            AsyncReadHead();
            return;
        }

        std::lock_guard<std::mutex> lock(_file_mutex);
        _zc_recv_state.received_size += size;

        nlohmann::json ack;
        ack["task_id"] = task_id;
        ack["received"] = _zc_recv_state.received_size;
        Send(ack.dump(), MSG_ZEROCOPY_READY);

        if (_zc_recv_state.received_size >= _zc_recv_state.total_size)
        {
            _zc_recv_state.fd.Reset();
            _zc_recv_state.receiving = false;

            nlohmann::json response{{"error", 0}, {"task_id", task_id}};
            Send(response.dump(), MSG_ZEROCOPY_COMPLETE);

            spdlog::info("[CSession] Zero-copy receive completed: task={}", task_id);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleZeroCopyData error: {}", e.what());
    }

    AsyncReadHead();
}

void CSession::HandleZeroCopyComplete(const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        int error = json_data.value("error", 0);

        if (error == 0)
        {
            spdlog::info("[CSession] Zero-copy send completed: task={}", task_id);
        }
        else
        {
            std::string message = json_data.value("message", "unknown error");
            spdlog::error("[CSession] Zero-copy send failed: task={}, error={}", task_id, message);
        }

        std::lock_guard<std::mutex> lock(_file_mutex);
        _zc_send_state.fd.Reset();
        _zc_send_state.sending = false;
        _zc_send_state.waiting_sendfile = false;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleZeroCopyComplete error: {}", e.what());
    }

    AsyncReadHead();
}

void CSession::HandleZeroCopyError(const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t task_id = json_data.value("task_id", 0);
        std::string message = json_data.value("message", "unknown error");

        spdlog::error("[CSession] Zero-copy error: task={}, message={}", task_id, message);

        std::lock_guard<std::mutex> lock(_file_mutex);
        _zc_send_state.fd.Reset();
        _zc_send_state.sending = false;
        _zc_send_state.waiting_sendfile = false;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleZeroCopyError parse error: {}", e.what());
    }

    AsyncReadHead();
}

void CSession::HandleZeroCopyReady(const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int error = json_data.value("error", 0);
        int64_t task_id = json_data.value("task_id", 0);

        if (error == 0 && task_id == _zc_send_state.task_id)
        {
            spdlog::info("[CSession] Zero-copy ready acknowledged, starting transfer: task={}", task_id);
            ContinueZeroCopySend();
        }
        else
        {
            std::string message = json_data.value("message", "rejected");
            spdlog::error("[CSession] Zero-copy ready rejected: task={}, message={}", task_id, message);

            std::lock_guard<std::mutex> lock(_file_mutex);
            _zc_send_state.fd.Reset();
            _zc_send_state.sending = false;
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CSession] HandleZeroCopyReady error: {}", e.what());
    }

    AsyncReadHead();
}

void CSession::StartZeroCopySend(int64_t task_id, const std::string &filepath)
{
    std::lock_guard<std::mutex> lock(_file_mutex);
    if (_zc_send_state.sending)
        return;

    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0)
    {
        spdlog::error("[CSession] Failed to open file for zero-copy send: {}", filepath);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        spdlog::error("[CSession] Failed to stat file: {}", filepath);
        return;
    }

    _zc_send_state.task_id = task_id;
    _zc_send_state.fd.Reset(fd);
    _zc_send_state.total_size = st.st_size;
    _zc_send_state.sent_size = 0;
    _zc_send_state.filename = filepath;
    _zc_send_state.sending = true;
    _zc_send_state.waiting_sendfile = false;

    nlohmann::json request;
    request["task_id"] = task_id;
    request["filename"] = filepath.substr(filepath.find_last_of('/') + 1);
    request["total_size"] = st.st_size;
    Send(request.dump(), MSG_ZEROCOPY_START);

    spdlog::info("[CSession] Zero-copy send initiated: task={}, file={}, size={}", task_id, filepath, st.st_size);
}

void CSession::ContinueZeroCopySend()
{
    std::lock_guard<std::mutex> lock(_file_mutex);

    if (!_zc_send_state.sending || _zc_send_state.fd < 0)
    {
        return;
    }

    if (_zc_send_state.sent_size >= _zc_send_state.total_size)
    {
        _zc_send_state.fd.Reset();
        _zc_send_state.sending = false;

        nlohmann::json complete;
        complete["task_id"] = _zc_send_state.task_id;
        complete["error"] = 0;
        Send(complete.dump(), MSG_ZEROCOPY_COMPLETE);

        spdlog::info("[CSession] Zero-copy send completed: task={}", _zc_send_state.task_id);
        OnZeroCopySendComplete(true, "transfer completed");
        return;
    }

    int64_t remain = _zc_send_state.total_size - _zc_send_state.sent_size;
    int64_t to_read = std::min(static_cast<int64_t>(CHUNK_SIZE), remain);

    char buffer[CHUNK_SIZE];
    ssize_t bytes_read = read(_zc_send_state.fd, buffer, to_read);

    if (bytes_read <= 0)
    {
        spdlog::error("[CSession] Zero-copy read error or EOF: task_id={}", _zc_send_state.task_id);
        _zc_send_state.fd.Reset();
        _zc_send_state.sending = false;

        nlohmann::json error_resp;
        error_resp["task_id"] = _zc_send_state.task_id;
        error_resp["error"] = 1;
        error_resp["message"] = "read error or EOF";
        Send(error_resp.dump(), MSG_ZEROCOPY_ERROR);

        OnZeroCopySendComplete(false, "read error");
        return;
    }

    std::vector<char> chunk_data(buffer, buffer + bytes_read);
    nlohmann::json json_meta;
    json_meta["task_id"] = _zc_send_state.task_id;
    json_meta["offset"] = _zc_send_state.sent_size;
    json_meta["size"] = bytes_read;

    std::string json_str = json_meta.dump();
    SendBinary(json_str, chunk_data, MSG_ZEROCOPY_DATA);

    _zc_send_state.sent_size += bytes_read;

    int progress = static_cast<int>((_zc_send_state.sent_size * 100) / _zc_send_state.total_size);
    spdlog::debug(
        "[CSession] Zero-copy send progress: task={}, sent={}/{}, progress={}%", _zc_send_state.task_id,
        _zc_send_state.sent_size, _zc_send_state.total_size, progress);

    if (_zc_send_state.sent_size < _zc_send_state.total_size)
    {
        auto self = shared_from_this();
        boost::asio::post(_strand, [this, self]() { ContinueZeroCopySend(); });
    }
    else
    {
        _zc_send_state.fd.Reset();
        _zc_send_state.sending = false;

        nlohmann::json complete;
        complete["task_id"] = _zc_send_state.task_id;
        complete["error"] = 0;
        Send(complete.dump(), MSG_ZEROCOPY_COMPLETE);

        spdlog::info("[CSession] Zero-copy send completed: task={}", _zc_send_state.task_id);
        OnZeroCopySendComplete(true, "transfer completed");
    }
}

void CSession::OnZeroCopySendComplete(bool success, const std::string &message)
{
    spdlog::info(
        "[CSession] Zero-copy send complete callback: task={}, success={}, message={}", _zc_send_state.task_id, success,
        message);
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
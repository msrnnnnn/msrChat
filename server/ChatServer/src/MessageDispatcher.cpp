/**
 * @file    MessageDispatcher.cpp
 * @brief   消息分发器实现
 * @details 注册并路由各类 protobuf 消息到对应的处理函数，
 *          涵盖登录认证、聊天文本、文件传输、图片消息、撤回/编辑等协议。
 */

#include "MessageDispatcher.h"
#include "CServer.h"
#include "CSession.h"
#include <cctype>
#include "FileTransfer.h"
#include "ImageStorage.h"
#include "Message.pb.h"
#include "MessageRouter.h"
#include "SessionManager.h"
#include "SQLiteMgr.h"
#include "AuthRepository.h"
#include "MessageRepository.h"
#include "TokenManager.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <string_view>

namespace
{

static int64_t NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool HandleLoginRequest(CSession &session, const std::string &body_data);
bool HandleRegisterRequest(CSession &session, const std::string &body_data);
bool HandleLoginAuthRequest(CSession &session, const std::string &body_data);
bool HandleGetVerifyCodeRequest(CSession &session, const std::string &body_data);
bool HandleResetPwdRequest(CSession &session, const std::string &body_data);
bool HandleChatText(CSession &session, const std::string &body_data);
bool HandleFileReq(CSession &session, const std::string &body_data);
bool HandleFileRsp(CSession &session, const std::string &body_data);
bool HandleFileChunk(CSession &session, const std::string &body_data);
bool HandleFileChunk(CSession &session, std::string_view body_view);
bool HandleFileAck(CSession &session, const std::string &body_data);
bool HandleOfflineAck(CSession &session, const std::string &body_data);
bool HandleChatImage(CSession &session, const std::string &body_data);
bool HandleImageDownloadReq(CSession &session, const std::string &body_data);
bool HandleChatRecall(CSession &session, const std::string &body_data);
bool HandleChatEdit(CSession &session, const std::string &body_data);
} // namespace

void MessageDispatcher::RegisterDefaultHandlers()
{
    RegisterHandler(MSG_HELLO,
        [](CSession &session, const std::string &body_data) -> bool {
            session.Send(body_data, MSG_HELLO);
            session.ContinueReading();
            return true;
        }, false);
    RegisterHandler(MSG_CHAT_LOGIN, HandleLoginRequest, false);
    RegisterHandler(ID_REGISTER_USER, HandleRegisterRequest, false);
    RegisterHandler(ID_LOGIN_USER, HandleLoginAuthRequest, false);
    RegisterHandler(ID_GET_VERIFY_CODE, HandleGetVerifyCodeRequest, false);
    RegisterHandler(ID_RESET_PWD, HandleResetPwdRequest, false);
    RegisterHandler(MSG_CHAT_TEXT, HandleChatText, true);
    RegisterHandler(MSG_FILE_REQ, HandleFileReq, true);
    RegisterHandler(MSG_FILE_RSP, HandleFileRsp, true);
    RegisterHandler(MSG_FILE_CHUNK,
        [](CSession &session, const std::string &body_data) -> bool {
            return HandleFileChunk(session, body_data);
        }, true);
    RegisterHandler(MSG_FILE_ACK, HandleFileAck, true);
    RegisterHandler(MSG_OFFLINE_ACK, HandleOfflineAck, true);
    RegisterHandler(MSG_CHAT_IMAGE, HandleChatImage, true);
    RegisterHandler(MSG_IMAGE_DOWNLOAD_REQ, HandleImageDownloadReq, true);
    RegisterHandler(MSG_CHAT_RECALL, HandleChatRecall, true);
    RegisterHandler(MSG_CHAT_EDIT, HandleChatEdit, true);
}

namespace
{

/**
 * @brief 聊天登录请求处理
 * @details 验证 Token 后调用 OnLoginValidated，失败则发送错误响应
 */
bool HandleLoginRequest(CSession &session, const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data, nullptr, false);
    nlohmann::json response;

    if (json_data.is_discarded())
    {
        response["error"] = ERR_JSON_PARSE;
        response["message"] = "invalid login payload";
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    int uid = json_data.value("uid", 0);
    std::string token = json_data.value("token", "");

    if (uid <= 0 || token.empty())
    {
        response["error"] = ERR_NETWORK;
        response["message"] = "invalid login";
        response["uid"] = uid;
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    if (session.GetUserUid() != 0)
    {
        response["error"] = ERR_NETWORK;
        response["message"] = "already login";
        response["uid"] = session.GetUserUid();
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    // CAS 原子操作：防止同一连接并发重复登录
    bool expected = false;
    if (!session.TrySetLoginInProgress(expected))
    {
        response["error"] = ERR_NETWORK;
        response["message"] = "login in progress";
        response["uid"] = uid;
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    auto server = session.GetServer();
    bool token_valid = false;
    if (server)
    {
        token_valid = TokenManager::Instance().CheckToken(uid, token);
    }
    session.OnLoginValidated(uid, token_valid);
    return true;
}

/**
 * @brief 用户注册请求处理
 * @details 异步验证验证码并写入数据库，返回 uid 和用户名
 */
bool HandleRegisterRequest(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string password_hash = json_data.value("passwd", "");
        std::string email = json_data.value("email", "");
        std::string verifycode = json_data.value("verifycode", "");

        if (username.empty() || password_hash.empty() || email.empty() || verifycode.empty())
        {
            nlohmann::json response{{"error", ERR_JSON_PARSE}};
            session.Send(response.dump(), ID_REGISTER_USER);
            session.ContinueReading();
            return true;
        }

        if (username.size() < 3 || username.size() > 20)
        {
            nlohmann::json response{{"error", ERR_JSON_PARSE}, {"message", "username must be 3-20 chars"}};
            session.Send(response.dump(), ID_REGISTER_USER);
            session.ContinueReading();
            return true;
        }
        for (char c : username)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                nlohmann::json response{{"error", ERR_JSON_PARSE}, {"message", "username invalid chars"}};
                session.Send(response.dump(), ID_REGISTER_USER);
                session.ContinueReading();
                return true;
            }
        }

        if (email.size() < 5 || email.size() > 254 || email.find('@') == std::string::npos
            || email.find('.') == std::string::npos)
        {
            nlohmann::json response{{"error", ERR_JSON_PARSE}, {"message", "invalid email"}};
            session.Send(response.dump(), ID_REGISTER_USER);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, username, password_hash, email, verifycode]()
            {
                if (safe_session->IsClosed()) return;

                int verifyResult = SQLiteMgr::Instance().Auth().CheckVerifyCode(email, verifycode);
                if (verifyResult != 0)
                {
                    nlohmann::json response;
                    response["error"] = verifyResult;
                    safe_session->Send(response.dump(), ID_REGISTER_USER);
                    safe_session->ContinueReading();
                    return;
                }

                AuthResult result = SQLiteMgr::Instance().Auth().RegisterUser(username, password_hash, email);

                nlohmann::json response;
                response["error"] = result.error;
                if (result.error == 0)
                {
                    response["uid"] = result.uid;
                    response["user"] = result.username;
                    response["token"] = result.token;
                    response["email"] = email;
                }
                safe_session->Send(response.dump(), ID_REGISTER_USER);
                safe_session->ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleRegisterRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_REGISTER_USER);
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleRegisterRequest unknown exception");
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_REGISTER_USER);
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 登录认证请求处理
 * @details 异步验证用户名密码，登录成功则分发 Token
 */
bool HandleLoginAuthRequest(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string password_hash = json_data.value("passwd", "");

        if (username.empty() || password_hash.empty())
        {
            nlohmann::json response;
            response["error"] = ERR_JSON_PARSE;
            response["message"] = "invalid parameters";
            session.Send(response.dump(), ID_LOGIN_USER);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, server, username, password_hash]()
            {
                if (safe_session->IsClosed()) return;

                AuthResult result = SQLiteMgr::Instance().Auth().LoginUser(username, password_hash);

                nlohmann::json response;
                response["error"] = result.error;
                if (result.error != 0)
                {
                    safe_session->Send(response.dump(), ID_LOGIN_USER);
                    safe_session->ContinueReading();
                    return;
                }

                TokenManager::Instance().SetToken(result.uid, result.token);
                response["uid"] = result.uid;
                response["user"] = result.username;
                response["token"] = result.token;
                spdlog::info("[MessageDispatcher] User {} auth login success, token issued", result.uid);

                safe_session->Send(response.dump(), ID_LOGIN_USER);
                safe_session->ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleLoginAuthRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_LOGIN_USER);
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleLoginAuthRequest unknown exception");
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_LOGIN_USER);
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 获取验证码请求处理
 * @details 异步发送验证码到指定邮箱
 */
bool HandleGetVerifyCodeRequest(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string email = json_data.value("email", "");

        if (email.empty())
        {
            nlohmann::json response{{"error", ERR_JSON_PARSE}};
            session.Send(response.dump(), ID_GET_VERIFY_CODE);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, email]()
            {
                if (safe_session->IsClosed()) return;

                int code = 0;
                bool success = SQLiteMgr::Instance().Auth().SendVerifyCode(email, code);

                nlohmann::json response{{"error", success ? ERR_SUCCESS : ERR_JSON_PARSE}, {"email", email}, {"code", code}};
                safe_session->Send(response.dump(), ID_GET_VERIFY_CODE);
                safe_session->ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleGetVerifyCodeRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_GET_VERIFY_CODE);
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleGetVerifyCodeRequest unknown exception");
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_GET_VERIFY_CODE);
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 重置密码请求处理
 * @details 先验证验证码，有效则更新密码
 */
bool HandleResetPwdRequest(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string email = json_data.value("email", "");
        std::string code = json_data.value("verifycode", "");
        std::string new_password_hash = json_data.value("passwd", "");

        if (username.empty() || email.empty() || code.empty() || new_password_hash.empty())
        {
            nlohmann::json response{{"error", ERR_JSON_PARSE}};
            session.Send(response.dump(), ID_RESET_PWD);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        auto safe_session = session.shared_from_this();
        server->GetThreadPool().Enqueue(
            [safe_session, username, email, code, new_password_hash]()
            {
                if (safe_session->IsClosed()) return;

                int verify_result = SQLiteMgr::Instance().Auth().CheckVerifyCode(email, code);
                int error_code = verify_result;
                std::string new_token;

                if (verify_result == 0)
                {
                    int reset_result = SQLiteMgr::Instance().Auth().ResetPassword(username, email, code, new_password_hash);
                    if (reset_result == 0)
                    {
                        error_code = 0;
                        auto user = SQLiteMgr::Instance().Auth().GetUserByUsername(username);
                        if (user.has_value())
                        {
                            int uid = user->uid;
                            AuthResult login_result = SQLiteMgr::Instance().Auth().LoginUser(username, new_password_hash);
                            if (login_result.error == 0)
                            {
                                new_token = login_result.token;
                                auto srv = safe_session->GetServer();
                                if (srv) TokenManager::Instance().SetToken(uid, new_token);
                            }
                        }
                    }
                    else
                    {
                        error_code = reset_result;
                    }
                }

                nlohmann::json response{{"error", error_code}};
                if (error_code == 0 && !new_token.empty())
                {
                    response["token"] = new_token;
                }
                safe_session->Send(response.dump(), ID_RESET_PWD);
                safe_session->ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleResetPwdRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_RESET_PWD);
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleResetPwdRequest unknown exception");
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_RESET_PWD);
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 聊天文本消息处理
 * @details 解析 Protobuf，验证后转发给目标用户，不在线则存离线消息
 */
bool HandleChatText(CSession &session, const std::string &body_data)
{
    std::string client_msg_id;
    try
    {
        // 解析 protobuf ChatTextMsg，失败则继续读取
        qmsrchat::ChatTextMsg chatMsg;
        if (!chatMsg.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse ChatTextMsg from Protobuf");
            session.ContinueReading();
            return true;
        }

        int from_uid = chatMsg.from_uid();
        int to_uid = chatMsg.to_uid();
        std::string content = chatMsg.content();
        client_msg_id = chatMsg.client_msg_id();

        if (session.GetUserUid() <= 0)
        {
            qmsrchat::ChatAck ack;
            ack.set_error(1);
            ack.set_message("not login");
            ack.set_client_msg_id(client_msg_id);

            std::string serialized;
            if (ack.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_CHAT_ACK);
            }
            session.ContinueReading();
            return true;
        }

        if (from_uid != 0 && from_uid != session.GetUserUid())
        {
            spdlog::warn("[MessageDispatcher] from_uid mismatch client: {} server: {}", from_uid, session.GetUserUid());
            qmsrchat::ChatAck ack;
            ack.set_error(1);
            ack.set_message("uid mismatch");
            ack.set_client_msg_id(client_msg_id);
            std::string serialized;
            if (ack.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_CHAT_ACK);
            }
            session.ContinueReading();
            return true;
        }

        if (to_uid <= 0 || content.empty() || content.size() > MAX_CHAT_CONTENT_LEN)
        {
            qmsrchat::ChatAck ack;
            ack.set_error(1);
            ack.set_message("invalid message");
            ack.set_client_msg_id(client_msg_id);

            std::string serialized;
            if (ack.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_CHAT_ACK);
            }
            session.ContinueReading();
            return true;
        }

        nlohmann::json forward;
        forward["from_uid"] = session.GetUserUid();
        forward["to_uid"] = to_uid;
        forward["content"] = content;
        if (!client_msg_id.empty())
        {
            forward["client_msg_id"] = client_msg_id;
        }
        if (chatMsg.timestamp() > 0)
        {
            forward["timestamp"] = chatMsg.timestamp();
        }

        // 尝试在线转发：直接用 Protobuf ServerChatMsg 发送（跳过 JSON 中间层）
        auto server = session.GetServer();
        bool delivered = false;
        bool stored = false;
        auto target_session = SessionManager::Instance().GetSession(to_uid);
        if (target_session)
        {
            qmsrchat::ServerChatMsg server_msg;
            server_msg.set_from_uid(session.GetUserUid());
            server_msg.set_to_uid(to_uid);
            server_msg.set_content(content);
            server_msg.set_client_msg_id(client_msg_id);
            server_msg.set_timestamp(chatMsg.timestamp() > 0 ? chatMsg.timestamp()
                : NowMs());
            std::string serialized;
            if (server_msg.SerializeToString(&serialized))
            {
                target_session->Send(serialized, MSG_CHAT_TEXT);
                delivered = true;
            }
        }
        if (!delivered && server)
        {
            std::string forward_data = forward.dump();
            stored = server->StoreOfflineMessage(to_uid, forward_data);
        }

        // 持久化到服务端 DB（撤回/编辑依赖 messages 表查询）
        ChatMessage db_msg;
        db_msg.from_uid = session.GetUserUid();
        db_msg.to_uid = to_uid;
        db_msg.content = content;
        db_msg.timestamp = chatMsg.timestamp() > 0 ? chatMsg.timestamp()
                                                    : NowMs();
        db_msg.status = delivered ? 1 : (stored ? 2 : 0);
        db_msg.client_msg_id = client_msg_id;
        db_msg.type = 0; // text
        SQLiteMgr::Instance().SaveMessage(db_msg);

        qmsrchat::ChatAck ack;
        if (delivered) {
            ack.set_error(0);
            ack.set_message("delivered");
        } else if (stored) {
            ack.set_error(0);
            ack.set_message("stored");
        } else {
            ack.set_error(1);
            ack.set_message("failed");
        }
        ack.set_client_msg_id(client_msg_id);

        std::string serialized;
        if (ack.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_CHAT_ACK);
        }
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleChatText error: {}", e.what());
        qmsrchat::ChatAck response;
        response.set_error(1);
        response.set_message("internal server error");
        if (!client_msg_id.empty())
        {
            response.set_client_msg_id(client_msg_id);
        }

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_CHAT_ACK);
        }
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleChatText unknown exception");
        qmsrchat::ChatAck response;
        response.set_error(1);
        response.set_message("internal server error");
        if (!client_msg_id.empty())
        {
            response.set_client_msg_id(client_msg_id);
        }
        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_CHAT_ACK);
        }
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 文件传输请求处理
 * @details 记录 P2P 路由映射并转发给接收方
 */
bool HandleFileReq(CSession &session, const std::string &body_data)
{
    if (body_data.size() > MAX_FILE_META_SIZE)
    {
        spdlog::warn("[MessageDispatcher] HandleFileReq: body too large {} > {}",
                     body_data.size(), MAX_FILE_META_SIZE);
        session.ContinueReading();
        return true;
    }

    if (session.GetUserUid() <= 0)
    {
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("not login");

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_FILE_ACK);
        }
        session.ContinueReading();
        return true;
    }

    try
    {
        qmsrchat::FileReq fileReq;
        if (!fileReq.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse FileReq from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = fileReq.task_id();
        int to_uid = fileReq.to_uid();
        std::string filename = fileReq.filename();
        int64_t total_size = fileReq.total_size();

        if (fileReq.from_uid() != session.GetUserUid())
        {
            spdlog::warn("[MessageDispatcher] HandleFileReq: from_uid mismatch {} vs session {}",
                         fileReq.from_uid(), session.GetUserUid());
            qmsrchat::FileAck response;
            response.set_error(1);
            response.set_message("uid mismatch");
            std::string serialized;
            if (response.SerializeToString(&serialized))
                session.Send(serialized, MSG_FILE_ACK);
            session.ContinueReading();
            return true;
        }

        if (task_id <= 0 || to_uid <= 0 || filename.empty() || total_size <= 0)
        {
            qmsrchat::FileAck response;
            response.set_error(1);
            response.set_message("invalid file request");

            std::string serialized;
            if (response.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_FILE_ACK);
            }
            session.ContinueReading();
            return true;
        }

        // 记录 P2P 路由映射，用于后续 chunk/ack 转发
        FileTransfer::Instance().AddTask(
            task_id, session.GetUserUid(), to_uid, filename, total_size);

        // ===== 图片模式检测 =====
        // 客户端以 "{uuid}.{ext}" 格式传 filename，image_id 即 UUID 部分
        // 匹配 UUID 格式后，预插入 ImageStorage 记录
        // Image mode detection: filename is "{uuid}.{ext}" → seed ImageStorage
        // (client uses image_id as task_id and encodes format in filename)
        if (filename.find('.') != std::string::npos)
        {
            size_t dot_pos = filename.find('.');
            std::string image_id = filename.substr(0, dot_pos);
            std::string ext = filename.substr(dot_pos + 1);
            // Validate UUID format (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, 36 chars with 4 dashes)
            if (image_id.size() == 36 && image_id[8] == '-' && image_id[13] == '-'
                && image_id[18] == '-' && image_id[23] == '-')
            {
                auto task = FileTransfer::Instance().GetTask(task_id);
                if (task)
                {
                    task->SetIsImage(true);
                    task->SetImageId(image_id);
                    ImageRecord rec;
                    rec.image_id = image_id;
                    rec.from_uid = session.GetUserUid();
                    rec.to_uid = to_uid;
                    rec.ext = ext;
                    rec.size = total_size;
                    rec.md5 = fileReq.md5();
                    rec.width = 0;
                    rec.height = 0;
                    rec.created_at = std::time(nullptr);
                    rec.expires_at = std::time(nullptr) + 7 * 24 * 3600;
                    if (!ImageStorage::Instance().Insert(rec))
                    {
                        spdlog::warn("HandleFileReq: ImageStorage Insert failed for {}", image_id);
                    }
                    else
                    {
                        spdlog::info("HandleFileReq: image mode, seeded image_storage for {}", image_id);
                    }
                }
            }
        }

        // 转发给接收方 B
        auto server = session.GetServer();
        if (server)
        {
            bool delivered = server->ForwardRawMessage(to_uid, MSG_FILE_REQ, body_data);
            if (!delivered)
            {
                auto task_ptr = FileTransfer::Instance().GetTask(task_id);
                if (task_ptr && task_ptr->IsImage())
                {
                    // 目标离线 + 图片模式：服务端代回 FileRsp，让发送方继续上传到 ImageStorage
                    task_ptr->SetTargetOffline(true);
                    spdlog::info("[MessageDispatcher] HandleFileReq: target uid={} offline, "
                                 "server acks for image upload, task_id={}", to_uid, task_id);

                    qmsrchat::FileRsp rsp;
                    rsp.set_task_id(task_id);
                    rsp.set_error(0);
                    rsp.set_offset(0);
                    rsp.set_message("server: target offline, uploading to storage");

                    std::string rsp_ser;
                    if (rsp.SerializeToString(&rsp_ser))
                    {
                        session.Send(rsp_ser, MSG_FILE_RSP);
                    }
                }
                else
                {
                    // 非图片模式：保持原有逻辑，报告目标离线
                    qmsrchat::FileAck response;
                    response.set_task_id(task_id);
                    response.set_error(1);
                    response.set_message("target user offline");

                    std::string serialized;
                    if (response.SerializeToString(&serialized))
                    {
                        session.Send(serialized, MSG_FILE_ACK);
                    }
                    FileTransfer::Instance().RemoveTask(task_id);
                }
            }
        }

        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleFileReq error: {}", e.what());
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("parse error");

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_FILE_ACK);
        }
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleFileReq unknown exception");
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 文件传输响应处理
 * @details 转发 FileRsp 给发送方（让其知晓接收方已准备好）
 */
bool HandleFileRsp(CSession &session, const std::string &body_data)
{
    try
    {
        qmsrchat::FileRsp fileRsp;
        if (!fileRsp.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse FileRsp from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = fileRsp.task_id();
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (!task)
        {
            spdlog::warn("[MessageDispatcher] FileRsp: task {} not found", task_id);
            session.ContinueReading();
            return true;
        }

        // 转发给发送方 A（服务端推送任务不需要转发）
        if (!task->IsTargetOffline())
        {
            int from_uid = task->GetFromUid();
            auto server = session.GetServer();
            if (server)
            {
                server->ForwardRawMessage(from_uid, MSG_FILE_RSP, body_data);
            }
            spdlog::info("[MessageDispatcher] FileRsp forwarded: task_id={}, from_uid={}", task_id, from_uid);
        }
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleFileRsp error: {}", e.what());
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleFileRsp unknown exception");
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 文件分片处理（string 重载）
 * @details 薄包装，将 string 转 string_view 后委托给实际处理函数
 */
bool HandleFileChunk(CSession &session, const std::string &body_data)
{
    return HandleFileChunk(session, std::string_view(body_data));
}

/**
 * @brief 文件分片处理（string_view 重载）
 */
bool HandleFileChunk(CSession &session, std::string_view body_view)
{
    if (session.GetUserUid() <= 0)
    {
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("not login");
        std::string serialized;
        if (response.SerializeToString(&serialized))
            session.Send(serialized, MSG_FILE_ACK);
        session.ContinueReading();
        return true;
    }

    try
    {
        qmsrchat::FileChunk chunk;
        if (!chunk.ParseFromArray(body_view.data(), static_cast<int>(body_view.size())))
        {
            spdlog::error("[MessageDispatcher] Failed to parse FileChunk");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = chunk.task_id();
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (!task)
        {
            spdlog::warn("[MessageDispatcher] FileChunk: task {} not found", task_id);
            session.ContinueReading();
            return true;
        }

        int to_uid = task->GetToUid();
        auto server = session.GetServer();
        if (server)
        {
            server->ForwardRawMessage(to_uid, MSG_FILE_CHUNK, std::string(body_view));
        }

        // Image mode: write chunk bytes to image_storage (in addition to forwarding)
        if (task->IsImage())
        {
            // 写入分片数据到 ImageStorage
            const std::string &data = chunk.data();
            if (!data.empty())
            {
                ImageStorage::Instance().AppendChunk(
                    task->GetImageId(),
                    chunk.offset(),
                    reinterpret_cast<const uint8_t *>(data.data()),
                    data.size());
            }

            // 目标离线时，服务端代回 FileAck 给发送方，让发送方继续发下一个 chunk
            if (task->IsTargetOffline())
            {
                int64_t chunk_end = chunk.offset() + static_cast<int64_t>(chunk.data().size());
                int from_uid = task->GetFromUid();
                auto srv = session.GetServer();
                if (srv)
                {
                    qmsrchat::FileAck ack;
                    ack.set_task_id(task_id);
                    ack.set_error(0);
                    ack.set_received(chunk_end);

                    if (chunk_end >= task->GetTotalSize())
                    {
                        ack.set_message("transfer complete");
                        ImageStorage::Instance().MarkCompleted(task->GetImageId());
                        spdlog::info("[MessageDispatcher] image upload complete (offline target): {}", task->GetImageId());
                        FileTransfer::Instance().RemoveTask(task_id);
                    }

                    std::string ack_ser;
                    if (ack.SerializeToString(&ack_ser))
                    {
                        srv->ForwardRawMessage(from_uid, MSG_FILE_ACK, ack_ser);
                    }
                }
            }
        }

        spdlog::debug("[MessageDispatcher] FileChunk forwarded: task_id={}, to_uid={}", task_id, to_uid);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleFileChunk error: {}", e.what());
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("chunk processing error");
        std::string serialized;
        if (response.SerializeToString(&serialized))
            session.Send(serialized, MSG_FILE_ACK);
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleFileChunk unknown exception");
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 文件传输 ACK 处理
 * @details 转发 ACK 给发送方，transfer complete 时清除任务
 */
bool HandleFileAck(CSession &session, const std::string &body_data)
{
    if (session.GetUserUid() <= 0)
    {
        session.ContinueReading();
        return true;
    }

    try
    {
        qmsrchat::FileAck fileAck;
        if (!fileAck.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse FileAck from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = fileAck.task_id();
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (!task)
        {
            spdlog::warn("[MessageDispatcher] FileAck: task {} not found", task_id);
            session.ContinueReading();
            return true;
        }

        // 转发给发送方 A（服务端推送任务不需要转发）
        if (!task->IsTargetOffline())
        {
            int from_uid = task->GetFromUid();
            auto server = session.GetServer();
            if (server)
            {
                server->ForwardRawMessage(from_uid, MSG_FILE_ACK, body_data);
            }
        }

        if (fileAck.received() >= task->GetTotalSize())
        {
            // Image mode: mark image_storage complete (sets expires_at = now + 7d)
            if (task->IsImage())
            {
                ImageStorage::Instance().MarkCompleted(task->GetImageId());
                spdlog::info("[MessageDispatcher] image upload complete: {}", task->GetImageId());
            }
            FileTransfer::Instance().RemoveTask(task_id);
            spdlog::info("[MessageDispatcher] File transfer completed (received={}, total={}), task_id={} removed",
                         fileAck.received(), task->GetTotalSize(), task_id);
        }

        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleFileAck error: {}", e.what());
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleFileAck unknown exception");
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 离线消息 ACK 处理
 * @details 触发 ContinueOfflineSend 继续发送下一页离线消息
 */
bool HandleOfflineAck(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t received = json_data.value("received", 0);
        spdlog::debug("[MessageDispatcher] Offline ack received={}", received);

        session.ContinueOfflineSend();
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleOfflineAck error: {}", e.what());
        session.ContinueReading();
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleOfflineAck unknown exception");
        session.ContinueReading();
    }
    return true;
}

/**
 * @brief 聊天图片消息处理
 * @details 解析 ImageMsg protobuf，在线则转发给接收方，
 *          离线则存入 SQLite（body_data 以 blob 存储），同时持久化到 messages 表
 */
bool HandleChatImage(CSession &session, const std::string &body_data)
{
    if (body_data.size() > MAX_CHUNK_SIZE)
    {
        spdlog::warn("[MessageDispatcher] HandleChatImage: body too large {} > {}",
                     body_data.size(), MAX_CHUNK_SIZE);
        session.ContinueReading();
        return true;
    }
    try
    {
    qmsrchat::ImageMsg msg;
    if (!msg.ParseFromString(body_data))
    {
        spdlog::error("[MessageDispatcher] HandleChatImage: parse failed");
        session.ContinueReading();
        return true;
    }

    int from = session.GetUserUid();
    if (from != msg.from_uid())
    {
        spdlog::warn("[MessageDispatcher] HandleChatImage: from_uid mismatch (session={}, msg={})",
                     from, msg.from_uid());
        qmsrchat::ChatAck ack;
        ack.set_error(1);
        ack.set_message("uid mismatch");
        ack.set_client_msg_id(msg.image_id());
        std::string serialized;
        if (ack.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_CHAT_ACK);
        }
        session.ContinueReading();
        return true;
    }

    auto target_session = SessionManager::Instance().GetSession(msg.to_uid());
    bool delivered = false;
    bool stored = false;
    if (target_session)
    {
        // 在线：直接发送 ImageMsg 给对方
        std::string serialized;
        msg.SerializeToString(&serialized);
        MessageRouter::Instance().SendToSession(target_session, serialized, MSG_CHAT_IMAGE);
        spdlog::info("[MessageDispatcher] HandleChatImage: forwarded to uid={}", msg.to_uid());
        delivered = true;
    }
    else
    {
        // 离线：存入 SQLite（内容用 blob 存 protobuf binary）
        auto server = session.GetServer();
        if (server)
        {
            ChatMessage offline_msg;
            offline_msg.from_uid = from;
            offline_msg.to_uid = msg.to_uid();
            offline_msg.type = 1;  // image
            offline_msg.image_id = msg.image_id();
            offline_msg.content = body_data;  // raw protobuf binary，用 blob 存储
            offline_msg.timestamp = msg.timestamp() > 0
                ? msg.timestamp()
                : NowMs();
            offline_msg.status = 2;  // offline stored
            offline_msg.client_msg_id = msg.image_id();
            stored = server->StoreOfflineMessage(offline_msg);
            spdlog::info("[MessageDispatcher] HandleChatImage: offline, stored in SQLite for uid={} (ok={})",
                         msg.to_uid(), stored);
        }
    }

    // 持久化到 messages 表（撤回/编辑/历史记录依赖）
    ChatMessage db_msg;
    db_msg.from_uid = from;
    db_msg.to_uid = msg.to_uid();
    db_msg.type = 1;
    db_msg.image_id = msg.image_id();
    db_msg.content = msg.caption();
    db_msg.timestamp = msg.timestamp() > 0
        ? msg.timestamp()
        : NowMs();
    db_msg.status = delivered ? 1 : (stored ? 2 : 0);
    db_msg.client_msg_id = msg.image_id();
    SQLiteMgr::Instance().SaveMessage(db_msg);

    // 回复 ACK
    qmsrchat::ChatAck ack;
    ack.set_error(0);
    ack.set_message(delivered ? "delivered" : (stored ? "stored" : "failed"));
    ack.set_client_msg_id(msg.image_id());
    std::string ack_data;
    ack.SerializeToString(&ack_data);
    session.Send(ack_data, MSG_CHAT_ACK);
    session.ContinueReading();
    return true;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleChatImage error: {}", e.what());
        session.ContinueReading();
        return true;
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleChatImage unknown exception");
        session.ContinueReading();
        return true;
    }
}

/**
 * @brief 图片下载请求处理
 * @details 查找 ImageStorage 记录，验证合法/未撤回后，以 FileReq + FileChunk + FileAck 协议推送图片数据
 */
bool HandleImageDownloadReq(CSession &session, const std::string &body_data)
{
    qmsrchat::ImageDownloadReq req;
    if (!req.ParseFromString(body_data))
    {
        spdlog::error("[MessageDispatcher] HandleImageDownloadReq: parse failed");
        session.ContinueReading();
        return true;
    }

    qmsrchat::ImageDownloadRsp rsp;
    rsp.set_image_id(req.image_id());

    auto rec = ImageStorage::Instance().Get(req.image_id());
    if (!rec.has_value())
    {
        rsp.set_error(ERR_IMAGE_EXPIRED);
        rsp.set_offset(0);
        spdlog::info("[MessageDispatcher] HandleImageDownloadReq: {} not found / expired",
                     req.image_id());

        std::string data;
        rsp.SerializeToString(&data);
        session.Send(data, MSG_IMAGE_DOWNLOAD_RSP);
        session.ContinueReading();
        return true;
    }
    if (rec->recalled)
    {
        rsp.set_error(ERR_IMAGE_EXPIRED);
        rsp.set_offset(0);
        spdlog::info("[MessageDispatcher] HandleImageDownloadReq: {} recalled",
                     req.image_id());

        std::string data;
        rsp.SerializeToString(&data);
        session.Send(data, MSG_IMAGE_DOWNLOAD_RSP);
        session.ContinueReading();
        return true;
    }

    // 预读取 blob 数据（在授权前验证数据可用性，避免空 blob 导致推送失败）
    std::vector<uint8_t> image_data;
    bool data_ok = (rec->size > 0) &&
                   ImageStorage::Instance().ReadRange(req.image_id(), 0, rec->size, image_data);

    if (!data_ok)
    {
        rsp.set_error(ERR_IMAGE_EXPIRED);
        rsp.set_offset(0);
        spdlog::info("[MessageDispatcher] HandleImageDownloadReq: {} blob unavailable (size={})",
                     req.image_id(), rec->size);

        std::string data;
        rsp.SerializeToString(&data);
        session.Send(data, MSG_IMAGE_DOWNLOAD_RSP);
        session.ContinueReading();
        return true;
    }

    // 授权成功
    rsp.set_error(0);
    rsp.set_offset(0);

    std::string rsp_data;
    rsp.SerializeToString(&rsp_data);
    session.Send(rsp_data, MSG_IMAGE_DOWNLOAD_RSP);
    spdlog::info("[MessageDispatcher] HandleImageDownloadReq: {} authorized, size={} ext={}",
                 req.image_id(), rec->size, rec->ext);

    // === 服务端主动推送文件（FileReq + FileChunk + FileAck）===
    // 生成唯一 task_id：对 image_id 取哈希，避免与其他传输任务冲突
    int64_t task_id = static_cast<int64_t>(std::hash<std::string>{}(req.image_id()) & 0x7FFFFFFFFFFFFFFFLL);
    std::string filename = req.image_id() + "." + rec->ext;
    int requester_uid = session.GetUserUid();

    // 注册 FileTransfer 任务，使 HandleFileRsp/HandleFileAck 能找到该任务
    // _target_offline=true 标记这是服务端推送（非 P2P 转发）
    FileTransfer::Instance().AddTask(task_id, rec->from_uid, requester_uid, filename, rec->size);
    {
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (task)
        {
            task->SetIsImage(true);
            task->SetImageId(req.image_id());
            task->SetTargetOffline(true);
        }
    }

    // 1. 发送 FileReq（触发客户端 FileRecvMgr::StartRecv）
    qmsrchat::FileReq fileReq;
    fileReq.set_task_id(task_id);
    fileReq.set_from_uid(rec->from_uid);
    fileReq.set_to_uid(requester_uid);
    fileReq.set_filename(filename);
    fileReq.set_total_size(rec->size);
    {
        std::string req_ser;
        fileReq.SerializeToString(&req_ser);
        session.Send(req_ser, MSG_FILE_REQ);
    }

    // 2. 分块发送 FileChunk（数据已在上方预读取）
    {
        constexpr int64_t kChunkSize = 4 * 1024;
        int64_t offset = 0;
        int64_t remaining = static_cast<int64_t>(image_data.size());
        while (remaining > 0)
        {
            int64_t this_chunk = std::min(kChunkSize, remaining);
            qmsrchat::FileChunk chunk;
            chunk.set_task_id(task_id);
            chunk.set_offset(offset);
            chunk.set_size(this_chunk);
            chunk.set_data(image_data.data() + offset, this_chunk);

            std::string chunk_ser;
            chunk.SerializeToString(&chunk_ser);
            session.Send(chunk_ser, MSG_FILE_CHUNK);

            offset += this_chunk;
            remaining -= this_chunk;
        }
        spdlog::info("[MessageDispatcher] HandleImageDownloadReq: pushed {} bytes in {} chunks",
                     image_data.size(), (image_data.size() + kChunkSize - 1) / kChunkSize);
    }

    // 3. 发送 FileAck（完成通知）
    qmsrchat::FileAck fileAck;
    fileAck.set_task_id(task_id);
    fileAck.set_error(0);
    fileAck.set_message("complete");
    {
        std::string ack_ser;
        fileAck.SerializeToString(&ack_ser);
        session.Send(ack_ser, MSG_FILE_ACK);
    }

    session.ContinueReading();
    return true;
}

/**
 * @brief 撤回消息（Phase 7 完整实现）
 * @details 流程：parse → 校验所有权 → 校验 2min 窗口 → 校验未已撤回 →
 *          标记 DB + 联动 ImageStorage（如图片）→ 推 RecallNotify → 回 ChatAck
 */
bool HandleChatRecall(CSession &session, const std::string &body_data)
{
    try
    {
    qmsrchat::RecallMsg req;
    if (!req.ParseFromString(body_data))
    {
        spdlog::warn("HandleChatRecall: parse failed");
        session.ContinueReading();
        return true;
    }
    const int from = session.GetUserUid();
    const int64_t now_ms = NowMs();

    // 1. 查原消息（用 timestamp + from_uid 精确查找）
    auto orig = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(req.msg_timestamp(), from);
    if (!orig.has_value())
    {
        spdlog::warn("HandleChatRecall: msg not found or not owner, ts={} from={}",
                     req.msg_timestamp(), from);
        qmsrchat::EditAck ack;
        ack.set_error(ERR_RECALL_NOT_OWNER);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_RECALL);
        session.ContinueReading();
        return true;
    }

    // 2. 2 分钟窗口校验
    if (now_ms - req.msg_timestamp() > 2LL * 60 * 1000)
    {
        spdlog::warn("HandleChatRecall: timeout, age_ms={}", now_ms - req.msg_timestamp());
        qmsrchat::EditAck ack;
        ack.set_error(ERR_RECALL_TIMEOUT);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_RECALL);
        session.ContinueReading();
        return true;
    }

    // 3. 已撤回检查
    if (orig->recalled)
    {
        spdlog::warn("HandleChatRecall: already recalled, ts={}", req.msg_timestamp());
        qmsrchat::EditAck ack;
        ack.set_error(ERR_MSG_ALREADY_RECALLED);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_RECALL);
        session.ContinueReading();
        return true;
    }

    // 4. 联动 ImageStorage（如图片）— P7 v1 已知限制：HandleChatImage 当前不写 messages 表，
    //    所以 type=1 的图片消息到这里 GetMessageByTimestamp 会返回 nullopt（被 NOT_OWNER 挡）。
    //    未来 HandleChatImage 加 SaveMessage(type=1, image_id) 后，下面的联动会生效。
    if (orig->type == 1 && !orig->image_id.empty())
    {
        ImageStorage::Instance().MarkRecalled(orig->image_id);
        spdlog::info("HandleChatRecall: marked image_storage recalled, image_id={}", orig->image_id);
    }

    // 5. DB 标记 recalled
    if (!SQLiteMgr::Instance().Messages().MarkMessageRecalled(req.msg_timestamp(), from, now_ms))
    {
        spdlog::error("HandleChatRecall: DB mark failed, ts={}", req.msg_timestamp());
        qmsrchat::EditAck ack;
        ack.set_error(1);  // 通用错误
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_RECALL);
        session.ContinueReading();
        return true;
    }

    // 6. 入 RecallNotifyQueue 兜底（无论目标是否在线）
    SQLiteMgr::Instance().EnqueueRecallNotify(orig->to_uid, req.msg_timestamp(), from, now_ms, orig->to_uid);
    spdlog::info("HandleChatRecall: Notify queued for uid={} ts={}", orig->to_uid, req.msg_timestamp());

    // 7. 尝试在线推送（优化路径）
    auto target_session = SessionManager::Instance().GetSession(orig->to_uid);
    if (target_session)
    {
        spdlog::info("HandleChatRecall: pushing 1014 to uid={} session={}", orig->to_uid, target_session->GetUuid());
        qmsrchat::RecallNotify n;
        n.set_msg_timestamp(req.msg_timestamp());
        n.set_recall_uid(from);
        n.set_recalled_to(orig->to_uid);
        n.set_recall_ts(now_ms);
        std::string s;
        n.SerializeToString(&s);
        if (!MessageRouter::Instance().SendToSession(target_session, s, MSG_CHAT_RECALL_NOTIFY))
        {
            spdlog::error("HandleChatRecall: SendToSession 1014 FAILED for uid={}", orig->to_uid);
        }
        else
        {
            spdlog::info("HandleChatRecall: SendToSession 1014 SUCCESS for uid={}", orig->to_uid);
            SQLiteMgr::Instance().ClearRecallNotifies(orig->to_uid);
        }
    }
    else
    {
        spdlog::info("HandleChatRecall: target uid={} offline, Notify queued (will deliver on login)", orig->to_uid);
    }

    // 7. 回 RecallAck 给发起方（用 EditAck 结构体，与 MSG_CHAT_RECALL 协议号配套）
    qmsrchat::EditAck ack;
    ack.set_error(0);
    ack.set_message("ok");
    ack.set_msg_timestamp(req.msg_timestamp());
    ack.set_edit_ts(now_ms);
    std::string s;
    ack.SerializeToString(&s);
    session.Send(s, MSG_CHAT_RECALL);
    session.ContinueReading();
    return true;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleChatRecall error: {}", e.what());
        session.ContinueReading();
        return true;
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleChatRecall unknown exception");
        session.ContinueReading();
        return true;
    }
}

/**
 * @brief 编辑消息（Phase 7 完整实现）
 * @details 流程：parse → 长度校验 → 校验所有权 → 校验 2min →
 *          DB 更新 → 推 EditNotify → 回 EditAck
 */
bool HandleChatEdit(CSession &session, const std::string &body_data)
{
    try
    {
    qmsrchat::EditMsg req;
    if (!req.ParseFromString(body_data))
    {
        spdlog::warn("HandleChatEdit: parse failed");
        session.ContinueReading();
        return true;
    }
    const int from = session.GetUserUid();
    const int64_t now_ms = NowMs();

    // 1. 长度校验
    if (req.new_content().size() > 2000)
    {
        qmsrchat::EditAck ack;
        ack.set_error(ERR_EDIT_TOO_LONG);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_EDIT);
        session.ContinueReading();
        return true;
    }

    // 2. 查原消息
    auto orig = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(req.msg_timestamp(), from);
    if (!orig.has_value())
    {
        spdlog::warn("HandleChatEdit: msg not found or not owner, ts={}", req.msg_timestamp());
        qmsrchat::EditAck ack;
        ack.set_error(ERR_EDIT_NOT_OWNER);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_EDIT);
        session.ContinueReading();
        return true;
    }

    // 3. 2 分钟窗口校验
    if (now_ms - req.msg_timestamp() > 2LL * 60 * 1000)
    {
        qmsrchat::EditAck ack;
        ack.set_error(ERR_EDIT_TIMEOUT);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_EDIT);
        session.ContinueReading();
        return true;
    }

    // 4. DB 更新 content + edited + edited_at
    if (!SQLiteMgr::Instance().Messages().UpdateMessageContent(req.msg_timestamp(), from,
                                                     req.new_content(), now_ms))
    {
        spdlog::error("HandleChatEdit: DB update failed, ts={}", req.msg_timestamp());
        qmsrchat::EditAck ack;
        ack.set_error(1);
        ack.set_msg_timestamp(req.msg_timestamp());
        std::string s;
        ack.SerializeToString(&s);
        session.Send(s, MSG_CHAT_EDIT);
        session.ContinueReading();
        return true;
    }

    // 5. 推 EditNotify 给原接收方
    auto target_session = SessionManager::Instance().GetSession(orig->to_uid);
    if (target_session)
    {
        qmsrchat::EditNotify n;
        n.set_msg_timestamp(req.msg_timestamp());
        n.set_from_uid(from);
        n.set_new_content(req.new_content());
        n.set_edit_ts(now_ms);
        std::string s;
        n.SerializeToString(&s);
        if (!MessageRouter::Instance().SendToSession(target_session, s, MSG_CHAT_EDIT_NOTIFY))
        {
            spdlog::warn("HandleChatEdit: Notify send failed");
        }
    }
    else
    {
        spdlog::info("HandleChatEdit: target uid={} offline, Notify dropped (P7 v1 limit)",
                     orig->to_uid);
    }

    // 6. 回 EditAck 给发起方（包含 new_content 供客户端更新本地UI）
    qmsrchat::EditAck ack;
    ack.set_error(0);
    ack.set_msg_timestamp(req.msg_timestamp());
    ack.set_edit_ts(now_ms);
    ack.set_new_content(req.new_content());
    std::string s;
    ack.SerializeToString(&s);
    session.Send(s, MSG_CHAT_EDIT);
    session.ContinueReading();
    return true;
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleChatEdit error: {}", e.what());
        session.ContinueReading();
        return true;
    }
    catch (...)
    {
        spdlog::error("[MessageDispatcher] HandleChatEdit unknown exception");
        session.ContinueReading();
        return true;
    }
}

} // namespace

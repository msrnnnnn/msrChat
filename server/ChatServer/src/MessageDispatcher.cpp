#include "MessageDispatcher.h"
#include "CServer.h"
#include "CSession.h"
#include "FileTransfer.h"
#include "ImageStorage.h"
#include "Message.pb.h"
#include "MessageRouter.h"
#include "SQLiteMgr.h"
#include "TokenManager.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <ctime>
#include <string_view>

namespace
{
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
    RegisterHandler(ID_GET_VARIFY_CODE, HandleGetVerifyCodeRequest, false);
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
        std::string verifycode = json_data.value("varifycode", "");

        if (username.empty() || password_hash.empty() || email.empty() || verifycode.empty())
        {
            nlohmann::json response{{"error", ERR_JSON_PARSE}};
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

                int verifyResult = SQLiteMgr::Instance().CheckVerifyCode(email, verifycode);
                if (verifyResult != 0)
                {
                    nlohmann::json response;
                    response["error"] = verifyResult;
                    safe_session->Send(response.dump(), ID_REGISTER_USER);
                    safe_session->ContinueReading();
                    return;
                }

                AuthResult result = SQLiteMgr::Instance().RegisterUser(username, password_hash, email);

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

                AuthResult result = SQLiteMgr::Instance().LoginUser(username, password_hash);

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
            session.Send(response.dump(), ID_GET_VARIFY_CODE);
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
                bool success = SQLiteMgr::Instance().SendVerifyCode(email, code);

                nlohmann::json response{{"error", success ? ERR_SUCCESS : ERR_JSON_PARSE}, {"email", email}, {"code", code}};
                safe_session->Send(response.dump(), ID_GET_VARIFY_CODE);
                safe_session->ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleGetVerifyCodeRequest error: {}", e.what());
        nlohmann::json response{{"error", ERR_JSON_PARSE}};
        session.Send(response.dump(), ID_GET_VARIFY_CODE);
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
        std::string code = json_data.value("varifycode", "");
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

                int verify_result = SQLiteMgr::Instance().CheckVerifyCode(email, code);
                int error_code = verify_result;
                std::string new_token;

                if (verify_result == 0)
                {
                    int reset_result = SQLiteMgr::Instance().ResetPassword(username, email, code, new_password_hash);
                    if (reset_result == 0)
                    {
                        error_code = 0;
                        auto user = SQLiteMgr::Instance().GetUserByUsername(username);
                        if (user.has_value())
                        {
                            int uid = user->uid;
                            AuthResult login_result = SQLiteMgr::Instance().LoginUser(username, new_password_hash);
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

        if (session.GetUserUid() == 0)
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

        std::string forward_data = forward.dump();
        auto server = session.GetServer();
        bool delivered = false;
        bool stored = false;
        if (server)
        {
            delivered = MessageRouter::Instance().ForwardMessage(to_uid, forward_data);
            if (!delivered)
            {
                stored = server->StoreOfflineMessage(to_uid, forward_data);
            }
        }

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
    return true;
}

/**
 * @brief 文件传输请求处理
 * @details 记录 P2P 路由映射并转发给接收方
 */
bool HandleFileReq(CSession &session, const std::string &body_data)
{
    if (session.GetUserUid() == 0)
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

        // 记录 P2P 路由映射，用于后续转发
        FileTransfer::Instance().AddTask(
            task_id, session.GetUserUid(), to_uid, filename, total_size);

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
                    rec.expires_at = std::time(nullptr);
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

        // 转发给发送方 A
        int from_uid = task->GetFromUid();
        auto server = session.GetServer();
        if (server)
        {
            server->ForwardRawMessage(from_uid, MSG_FILE_RSP, body_data);
        }

        spdlog::info("[MessageDispatcher] FileRsp forwarded: task_id={}, from_uid={}", task_id, from_uid);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleFileRsp error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

bool HandleFileChunk(CSession &session, const std::string &body_data)
{
    return HandleFileChunk(session, std::string_view(body_data));
}

/**
 * @brief 文件分片处理（string_view 重载）
 */
bool HandleFileChunk(CSession &session, std::string_view body_view)
{
    if (session.GetUserUid() == 0)
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
        if (!chunk.ParseFromString(std::string(body_view)))
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
            const std::string &data = chunk.data();
            if (!data.empty())
            {
                ImageStorage::Instance().AppendChunk(
                    task->GetImageId(),
                    chunk.offset(),
                    reinterpret_cast<const uint8_t *>(data.data()),
                    data.size());
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
    return true;
}

/**
 * @brief 文件传输 ACK 处理
 * @details 转发 ACK 给发送方，transfer complete 时清除任务
 */
bool HandleFileAck(CSession &session, const std::string &body_data)
{
    if (session.GetUserUid() == 0)
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

        // 转发给发送方 A
        int from_uid = task->GetFromUid();
        auto server = session.GetServer();
        if (server)
        {
            server->ForwardRawMessage(from_uid, MSG_FILE_ACK, body_data);
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
    return true;
}

bool HandleChatImage(CSession &session, const std::string &)
{
    spdlog::warn("HandleChatImage: stub (Phase 2 implements)");
    session.ContinueReading();
    return true;
}

bool HandleImageDownloadReq(CSession &session, const std::string &)
{
    spdlog::warn("HandleImageDownloadReq: stub (Phase 2 implements)");
    session.ContinueReading();
    return true;
}

bool HandleChatRecall(CSession &session, const std::string &)
{
    spdlog::warn("HandleChatRecall: stub (Phase 7 implements)");
    session.ContinueReading();
    return true;
}

bool HandleChatEdit(CSession &session, const std::string &)
{
    spdlog::warn("HandleChatEdit: stub (Phase 7 implements)");
    session.ContinueReading();
    return true;
}

} // namespace

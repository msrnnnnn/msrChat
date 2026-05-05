#include "MessageDispatcher.h"
#include "CServer.h"
#include "CSession.h"
#include "Message.pb.h"
#include "SQLiteMgr.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
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
bool HandleZeroCopyStart(CSession &session, const std::string &body_data);
bool HandleZeroCopyReady(CSession &session, const std::string &body_data);
bool HandleZeroCopyData(CSession &session, const std::string &body_data);
bool HandleZeroCopyComplete(CSession &session, const std::string &body_data);
bool HandleZeroCopyError(CSession &session, const std::string &body_data);
} // namespace

void MessageDispatcher::RegisterDefaultHandlers()
{
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
    RegisterHandler(MSG_ZEROCOPY_START, HandleZeroCopyStart, true);
    RegisterHandler(MSG_ZEROCOPY_READY, HandleZeroCopyReady, true);
    RegisterHandler(MSG_ZEROCOPY_DATA, HandleZeroCopyData, true);
    RegisterHandler(MSG_ZEROCOPY_COMPLETE, HandleZeroCopyComplete, true);
    RegisterHandler(MSG_ZEROCOPY_ERROR, HandleZeroCopyError, true);
}

namespace
{

bool HandleLoginRequest(CSession &session, const std::string &body_data)
{
    auto json_data = nlohmann::json::parse(body_data, nullptr, false);
    nlohmann::json response;

    if (json_data.is_discarded())
    {
        response["error"] = 1;
        response["message"] = "invalid login payload";
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    int uid = json_data.value("uid", 0);
    std::string token = json_data.value("token", "");

    if (uid <= 0 || token.empty())
    {
        response["error"] = 1;
        response["message"] = "invalid login";
        response["uid"] = uid;
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    if (session.GetUserUid() != 0)
    {
        response["error"] = 1;
        response["message"] = "already login";
        response["uid"] = session.GetUserUid();
        session.Send(response.dump(), MSG_CHAT_LOGIN);
        session.ContinueReading();
        return true;
    }

    bool expected = false;
    if (!session.TrySetLoginInProgress(expected))
    {
        response["error"] = 1;
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
        token_valid = server->CheckToken(uid, token);
    }
    session.OnLoginValidated(uid, token_valid);
    return true;
}

bool HandleRegisterRequest(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string username = json_data.value("user", "");
        std::string password_hash = json_data.value("passwd", "");
        std::string email = json_data.value("email", "");

        if (username.empty() || password_hash.empty() || email.empty())
        {
            nlohmann::json response{{"error", 1}};
            session.Send(response.dump(), ID_REGISTER_USER);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        server->GetThreadPool().Enqueue(
            [&session, username, password_hash, email]()
            {
                AuthResult result = SQLiteMgr::Instance().RegisterUser(username, password_hash, email);

                nlohmann::json response;
                response["error"] = result.error;
                if (result.error == 0)
                {
                    response["uid"] = result.uid;
                    response["username"] = result.username;
                }
                session.Send(response.dump(), ID_REGISTER_USER);
                session.ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleRegisterRequest error: {}", e.what());
        nlohmann::json response{{"error", 1}};
        session.Send(response.dump(), ID_REGISTER_USER);
        session.ContinueReading();
    }
    return true;
}

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
            response["error"] = 1;
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

        server->GetThreadPool().Enqueue(
            [&session, server, username, password_hash]()
            {
                AuthResult result = SQLiteMgr::Instance().LoginUser(username, password_hash);

                nlohmann::json response;
                response["error"] = result.error;
                if (result.error != 0)
                {
                    session.Send(response.dump(), ID_LOGIN_USER);
                    session.ContinueReading();
                    return;
                }

                server->SetToken(result.uid, result.token);
                response["uid"] = result.uid;
                response["username"] = result.username;
                response["token"] = result.token;
                spdlog::info("[MessageDispatcher] User {} auth login success, token issued", result.uid);

                session.Send(response.dump(), ID_LOGIN_USER);
                session.ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleLoginAuthRequest error: {}", e.what());
        nlohmann::json response{{"error", 1}};
        session.Send(response.dump(), ID_LOGIN_USER);
        session.ContinueReading();
    }
    return true;
}

bool HandleGetVerifyCodeRequest(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        std::string email = json_data.value("email", "");

        if (email.empty())
        {
            nlohmann::json response{{"error", 1}};
            session.Send(response.dump(), ID_GET_VARIFY_CODE);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        server->GetThreadPool().Enqueue(
            [&session, email]()
            {
                bool success = SQLiteMgr::Instance().SendVerifyCode(email);

                nlohmann::json response{{"error", success ? 0 : 1}};
                session.Send(response.dump(), ID_GET_VARIFY_CODE);
                session.ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleGetVerifyCodeRequest error: {}", e.what());
        nlohmann::json response{{"error", 1}};
        session.Send(response.dump(), ID_GET_VARIFY_CODE);
        session.ContinueReading();
    }
    return true;
}

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
            nlohmann::json response{{"error", 1}};
            session.Send(response.dump(), ID_RESET_PWD);
            session.ContinueReading();
            return true;
        }

        auto server = session.GetServer();
        if (!server)
        {
            return true;
        }

        server->GetThreadPool().Enqueue(
            [&session, username, email, code, new_password_hash]()
            {
                int verify_result = SQLiteMgr::Instance().CheckVerifyCode(email, code);
                bool success = false;
                int error_code = verify_result;

                if (verify_result == 0)
                {
                    success = SQLiteMgr::Instance().ResetPassword(username, email, code, new_password_hash);
                    error_code = success ? 0 : 1009;
                }

                nlohmann::json response{{"error", error_code}};
                session.Send(response.dump(), ID_RESET_PWD);
                session.ContinueReading();
            });
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleResetPwdRequest error: {}", e.what());
        nlohmann::json response{{"error", 1}};
        session.Send(response.dump(), ID_RESET_PWD);
        session.ContinueReading();
    }
    return true;
}

bool HandleChatText(CSession &session, const std::string &body_data)
{
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
        std::string client_msg_id = chatMsg.client_msg_id();

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
        if (server)
        {
            delivered = server->ForwardMessage(to_uid, forward_data);
            if (!delivered)
            {
                server->StoreOfflineMessage(to_uid, forward_data);
            }
        }

        qmsrchat::ChatAck ack;
        ack.set_error(delivered ? 0 : 1);
        ack.set_message(delivered ? "delivered" : "stored");
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

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_CHAT_ACK);
        }
        session.ContinueReading();
    }
    return true;
}

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

        session.PrepareFileReceive(task_id, to_uid, filename, total_size);

        qmsrchat::FileAck response;
        response.set_error(0);
        response.set_task_id(task_id);
        response.set_message("ready to receive");

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_FILE_ACK);
        }

        nlohmann::json forward;
        forward["task_id"] = task_id;
        forward["from_uid"] = session.GetUserUid();
        forward["filename"] = filename;
        forward["total_size"] = total_size;
        auto server = session.GetServer();
        if (server)
        {
            server->ForwardMessage(to_uid, forward.dump());
        }

        spdlog::info(
            "[MessageDispatcher] File transfer ready: task={}, file={}, size={}", task_id, filename, total_size);
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
        int error = fileRsp.error();
        std::string message = fileRsp.message();

        spdlog::info("[MessageDispatcher] File rsp: task_id={}, error={}, message={}", task_id, error, message);
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

bool HandleFileChunk(CSession &session, std::string_view body_view)
{
    if (session.GetUserUid() == 0)
    {
        nlohmann::json response{{"error", 1}, {"message", "not login"}};
        session.Send(response.dump(), MSG_FILE_ACK);
        session.ContinueReading();
        return true;
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

        bool transfer_ready = session.IsFileTransferReady(task_id);
        if (!transfer_ready)
        {
            spdlog::warn("[MessageDispatcher] File chunk received without proper setup, task_id={}", task_id);
            session.ContinueReading();
            return true;
        }

        if (!chunk_view.empty())
        {
            session.AppendFileChunk(task_id, chunk_view);
        }
        else if (chunk_size > 0 && static_cast<int64_t>(body_view.size()) > chunk_size)
        {
            size_t actual_data_start = body_view.size() - static_cast<size_t>(chunk_size);
            std::string_view data_view = body_view.substr(actual_data_start);
            session.AppendFileChunk(task_id, data_view);
        }

        int progress = session.GetFileTransferProgress(task_id);
        spdlog::debug("[MessageDispatcher] File chunk: task={}, progress={}%", task_id, progress);

        if (session.IsFileTransferComplete(task_id))
        {
            spdlog::info("[MessageDispatcher] File transfer completed: task_id={}", task_id);

            nlohmann::json response{{"error", 0}, {"task_id", task_id}, {"message", "transfer complete"}};
            session.Send(response.dump(), MSG_FILE_ACK);

            session.FinishFileReceive(task_id);
        }
        else
        {
            int64_t received = session.GetReceivedFileSize(task_id);
            nlohmann::json response{{"error", 0}, {"task_id", task_id}, {"received", received}};
            session.Send(response.dump(), MSG_FILE_ACK);
        }

        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleFileChunk error: {}", e.what());
        nlohmann::json response{{"error", 1}, {"message", "chunk processing error"}};
        session.Send(response.dump(), MSG_FILE_ACK);
        session.ContinueReading();
    }
    return true;
}

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
        int error = fileAck.error();
        std::string message = fileAck.message();
        int64_t received = fileAck.received();

        if (error == 0 && message == "ready to receive")
        {
            session.StartFileSend(task_id);
        }
        else if (error == 0 && received > 0)
        {
            session.UpdateFileSendProgress(task_id, received);
        }
        else if (message == "transfer complete")
        {
            session.FinishFileSend(task_id);
        }
        else if (error != 0)
        {
            spdlog::warn("[MessageDispatcher] File send error: task_id={}, error={}", task_id, error);
            session.CancelFileSend(task_id);
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

bool HandleOfflineAck(CSession &session, const std::string &body_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t page = json_data.value("page", 0);
        spdlog::debug("[MessageDispatcher] Offline ack page: {}", page);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleOfflineAck error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

bool HandleZeroCopyStart(CSession &session, const std::string &body_data)
{
    if (session.GetUserUid() == 0)
    {
        qmsrchat::ZeroCopyReady response;
        response.set_error(1);
        response.set_message("not login");

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_ZEROCOPY_START);
        }
        session.ContinueReading();
        return true;
    }

    try
    {
        qmsrchat::ZeroCopyStart zcStart;
        if (!zcStart.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse ZeroCopyStart from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = zcStart.task_id();
        int to_uid = zcStart.to_uid();
        std::string filename = zcStart.filename();
        int64_t total_size = zcStart.total_size();

        spdlog::info("[MessageDispatcher] ZeroCopy start: task_id={}, to_uid={}, file={}", task_id, to_uid, filename);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleZeroCopyStart error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

bool HandleZeroCopyReady(CSession &session, const std::string &body_data)
{
    try
    {
        qmsrchat::ZeroCopyReady zcReady;
        if (!zcReady.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse ZeroCopyReady from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = zcReady.task_id();
        spdlog::info("[MessageDispatcher] ZeroCopy ready: task_id={}", task_id);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleZeroCopyReady error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

bool HandleZeroCopyData(CSession &session, const std::string &body_data)
{
    try
    {
        qmsrchat::ZeroCopyData zcData;
        if (!zcData.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse ZeroCopyData from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = zcData.task_id();
        spdlog::debug("[MessageDispatcher] ZeroCopy data: task_id={}", task_id);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleZeroCopyData error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

bool HandleZeroCopyComplete(CSession &session, const std::string &body_data)
{
    try
    {
        qmsrchat::ZeroCopyComplete zcComplete;
        if (!zcComplete.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse ZeroCopyComplete from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = zcComplete.task_id();
        spdlog::info("[MessageDispatcher] ZeroCopy complete: task_id={}", task_id);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleZeroCopyComplete error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

bool HandleZeroCopyError(CSession &session, const std::string &body_data)
{
    try
    {
        qmsrchat::ZeroCopyError zcError;
        if (!zcError.ParseFromString(body_data))
        {
            spdlog::error("[MessageDispatcher] Failed to parse ZeroCopyError from Protobuf");
            session.ContinueReading();
            return true;
        }

        int64_t task_id = zcError.task_id();
        std::string message = zcError.message();
        spdlog::warn("[MessageDispatcher] ZeroCopy error: task_id={}, message={}", task_id, message);
        session.ContinueReading();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[MessageDispatcher] HandleZeroCopyError error: {}", e.what());
        session.ContinueReading();
    }
    return true;
}

} // namespace

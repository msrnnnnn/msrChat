/**
 * @file    ChatService.cpp
 * @brief   聊天相关消息处理器实现
 * @details 文本消息、离线消息 ACK
 */

#include "services/ChatService.h"
#include "services/DispatchGuard.h"
#include "CServer.h"
#include "SessionManager.h"
#include "MessageRouter.h"
#include "SQLiteMgr.h"
#include "MessageRepository.h"
#include "RateLimiter.h"
#include "NonceCache.h"
#include "Message.pb.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace
{

static int64_t NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

// ─── HandleChatText (MSG_CHAT_TEXT 1006) ───

bool ChatService::HandleChatText(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);
    std::string client_msg_id;

    try
    {
        // 解析 protobuf ChatTextMsg
        qmsrchat::ChatTextMsg chatMsg;
        if (!chatMsg.ParseFromString(body_data))
        {
            spdlog::error("[ChatService] Failed to parse ChatTextMsg from Protobuf");
            return true;
        }

        int from_uid = chatMsg.from_uid();
        int to_uid = chatMsg.to_uid();
        std::string content = chatMsg.content();
        client_msg_id = chatMsg.client_msg_id();

        // Phase 5E: 防重放 Nonce 校验
        if (chatMsg.has_nonce_header()) {
            const auto &nh = chatMsg.nonce_header();
            if (!NonceCache::Instance().IsWithinTimeWindow(nh.timestamp())) {
                spdlog::warn("[ChatService] Nonce time window exceeded for uid={}", session.GetUserUid());
                return true;
            }
            if (!NonceCache::Instance().TryInsert(nh.nonce())) {
                spdlog::warn("[ChatService] Duplicate nonce detected for uid={}", session.GetUserUid());
                return true;
            }
            // 7E: HMAC 签名校验（兼容旧客户端：signature 为空时跳过）
            if (!nh.signature().empty()) {
                std::string expected = ComputeHmacSha256(
                    std::string(HMAC_KEY),
                    nh.nonce() + std::to_string(nh.timestamp()));
                if (nh.signature() != expected) {
                    spdlog::warn("[ChatService] HMAC signature mismatch for uid={}", session.GetUserUid());
                    return true;
                }
            }
        }

        // Phase 6: schema_version 校验（0 = 未设置，视为 v1 兼容）
        if (chatMsg.schema_version() != 0 && chatMsg.schema_version() != SCHEMA_VERSION) {
            spdlog::warn("[ChatService] Unsupported schema_version={} from uid={}",
                         chatMsg.schema_version(), session.GetUserUid());
            return true;
        }

        if (!client_msg_id.empty() && SQLiteMgr::Instance().Messages().MessageExists(client_msg_id))
        {
            qmsrchat::ChatAck ack;
            ack.set_error(0);
            ack.set_message("duplicate");
            ack.set_client_msg_id(client_msg_id);
            std::string serialized;
            if (ack.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_CHAT_ACK);
            }
            return true;
        }

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
            return true;
        }

        // 限流检查
        if (!RateLimiter::Instance().TryAcquire(session.GetUserUid()))
        {
            spdlog::warn("[ChatService] Rate limited for uid={}", session.GetUserUid());
            qmsrchat::ChatAck ack;
            ack.set_error(ERR_RATE_LIMITED);
            ack.set_message("rate limited");
            ack.set_client_msg_id(client_msg_id);

            std::string serialized;
            if (ack.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_CHAT_ACK);
            }
            return true;
        }

        if (from_uid != 0 && from_uid != session.GetUserUid())
        {
            spdlog::warn("[ChatService] from_uid mismatch client: {} server: {}", from_uid, session.GetUserUid());
            qmsrchat::ChatAck ack;
            ack.set_error(1);
            ack.set_message("uid mismatch");
            ack.set_client_msg_id(client_msg_id);
            std::string serialized;
            if (ack.SerializeToString(&serialized))
            {
                session.Send(serialized, MSG_CHAT_ACK);
            }
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
        SQLiteMgr::Instance().Messages().SaveMessage(db_msg);

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
    }
    catch (const std::exception &e)
    {
        spdlog::error("[ChatService] HandleChatText error: {}", e.what());
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
    }
    catch (...)
    {
        spdlog::error("[ChatService] HandleChatText unknown exception");
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
    }
    return true;
}

// ─── HandleOfflineAck (MSG_OFFLINE_ACK 1008) ───

bool ChatService::HandleOfflineAck(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        auto json_data = nlohmann::json::parse(body_data);
        int64_t received = json_data.value("received", 0);
        spdlog::debug("[ChatService] Offline ack received={}", received);

        session.ContinueOfflineSend();
    }
    catch (const std::exception &e)
    {
        spdlog::error("[ChatService] HandleOfflineAck error: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("[ChatService] HandleOfflineAck unknown exception");
    }
    return true;
}

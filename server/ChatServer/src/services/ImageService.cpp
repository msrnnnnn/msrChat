/**
 * @file    ImageService.cpp
 * @brief   图片 + 撤回/编辑消息处理器实现
 * @details 图片消息、图片下载、消息撤回、消息编辑
 */

#include "services/ImageService.h"
#include "services/DispatchGuard.h"
#include "CServer.h"
#include "CSession.h"
#include "SessionManager.h"
#include "MessageRouter.h"
#include "FileTransfer.h"
#include "ImageStorage.h"
#include "SQLiteMgr.h"
#include "MessageRepository.h"
#include "NonceCache.h"
#include "Message.pb.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <ctime>
#include <functional>

namespace
{

static int64_t NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/// 发送 EditAck 响应（recall / edit 共用）
void SendEditAck(CSession &session, int msg_type, int error, int64_t msg_timestamp,
                 int64_t edit_ts = 0, const std::string &content = "",
                 const std::string &message = "")
{
    qmsrchat::EditAck ack;
    ack.set_error(error);
    ack.set_msg_timestamp(msg_timestamp);
    if (edit_ts > 0) ack.set_edit_ts(edit_ts);
    if (!content.empty()) ack.set_new_content(content);
    if (!message.empty()) ack.set_message(message);
    std::string s;
    if (!ack.SerializeToString(&s))
    {
        spdlog::error("[ImageService] SendEditAck: SerializeToString failed");
        return;
    }
    session.Send(s, msg_type);
}

/// 向目标会话推送 RecallNotify，成功时精确删除该通知
void PushRecallNotify(int to_uid, int from_uid, int64_t msg_ts, int64_t recall_ts)
{
    auto target = SessionManager::Instance().GetSession(to_uid);
    if (target)
    {
        spdlog::info("HandleChatRecall: pushing 1014 to uid={} session={}", to_uid, target->GetUuid());
        qmsrchat::RecallNotify n;
        n.set_msg_timestamp(msg_ts);
        n.set_recall_uid(from_uid);
        n.set_recalled_to(to_uid);
        n.set_recall_ts(recall_ts);
        std::string s;
        if (!n.SerializeToString(&s))
        {
            spdlog::error("[ImageService] PushRecallNotify: SerializeToString failed");
            return;
        }
        if (!MessageRouter::Instance().SendToSession(target, s, MSG_CHAT_RECALL_NOTIFY))
            spdlog::error("HandleChatRecall: SendToSession 1014 FAILED for uid={}", to_uid);
        else
        {
            spdlog::info("HandleChatRecall: SendToSession 1014 SUCCESS for uid={}", to_uid);
            SQLiteMgr::Instance().Messages().ClearRecallNotifyByTimestamp(to_uid, msg_ts);
        }
    }
    else
    {
        spdlog::info("HandleChatRecall: target uid={} offline, Notify queued (will deliver on login)", to_uid);
    }
}

} // namespace

// ─── HandleChatImage (MSG_CHAT_IMAGE 1009) ───

bool ImageService::HandleChatImage(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    if (body_data.size() > MAX_CHUNK_SIZE)
    {
        spdlog::warn("[ImageService] HandleChatImage: body too large {} > {}",
                     body_data.size(), MAX_CHUNK_SIZE);
        return true;
    }

    try
    {
        qmsrchat::ImageMsg msg;
        if (!msg.ParseFromString(body_data))
        {
            spdlog::error("[ImageService] HandleChatImage: parse failed");
            return true;
        }

        int from = session.GetUserUid();
        if (from != msg.from_uid())
        {
            spdlog::warn("[ImageService] HandleChatImage: from_uid mismatch (session={}, msg={})",
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
            return true;
        }

        auto target_session = SessionManager::Instance().GetSession(msg.to_uid());
        bool delivered = false;
        bool stored = false;
        if (target_session)
        {
            // 在线：直接发送 ImageMsg 给对方
            std::string serialized;
            if (!msg.SerializeToString(&serialized))
            {
                spdlog::error("[ImageService] HandleChatImage: serialize failed for forwarding");
            }
            else
            {
                MessageRouter::Instance().SendToSession(target_session, serialized, MSG_CHAT_IMAGE);
                spdlog::info("[ImageService] HandleChatImage: forwarded to uid={}", msg.to_uid());
                delivered = true;
            }
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
                offline_msg.content = body_data;  // raw protobuf binary
                offline_msg.timestamp = msg.timestamp() > 0
                    ? msg.timestamp()
                    : NowMs();
                offline_msg.status = 2;  // offline stored
                offline_msg.client_msg_id = msg.image_id();
                stored = server->StoreOfflineMessage(offline_msg);
                spdlog::info("[ImageService] HandleChatImage: offline, stored in SQLite for uid={} (ok={})",
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
        SQLiteMgr::Instance().Messages().SaveMessage(db_msg);

        // 回复 ACK
        qmsrchat::ChatAck ack;
        ack.set_error(0);
        ack.set_message(delivered ? "delivered" : (stored ? "stored" : "failed"));
        ack.set_client_msg_id(msg.image_id());
        std::string ack_data;
        if (!ack.SerializeToString(&ack_data))
        {
            spdlog::error("[ImageService] HandleChatImage: ack serialize failed");
        }
        else
        {
            session.Send(ack_data, MSG_CHAT_ACK);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[ImageService] HandleChatImage error: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("[ImageService] HandleChatImage unknown exception");
    }
    return true;
}

// ─── HandleImageDownloadReq (MSG_IMAGE_DOWNLOAD_REQ 1013) ───

bool ImageService::HandleImageDownloadReq(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    qmsrchat::ImageDownloadReq req;
    if (!req.ParseFromString(body_data))
    {
        spdlog::error("[ImageService] HandleImageDownloadReq: parse failed");
        return true;
    }

    qmsrchat::ImageDownloadRsp rsp;
    rsp.set_image_id(req.image_id());

    auto rec = ImageStorage::Instance().Get(req.image_id());
    if (!rec.has_value())
    {
        rsp.set_error(ERR_IMAGE_EXPIRED);
        rsp.set_offset(0);
        spdlog::info("[ImageService] HandleImageDownloadReq: {} not found / expired",
                     req.image_id());

        std::string data;
        if (!rsp.SerializeToString(&data))
        {
            spdlog::error("[ImageService] HandleImageDownloadReq: serialize failed (not found)");
            return true;
        }
        session.Send(data, MSG_IMAGE_DOWNLOAD_RSP);
        return true;
    }
    if (rec->recalled)
    {
        rsp.set_error(ERR_IMAGE_EXPIRED);
        rsp.set_offset(0);
        spdlog::info("[ImageService] HandleImageDownloadReq: {} recalled",
                     req.image_id());

        std::string data;
        if (!rsp.SerializeToString(&data))
        {
            spdlog::error("[ImageService] HandleImageDownloadReq: serialize failed (recalled)");
            return true;
        }
        session.Send(data, MSG_IMAGE_DOWNLOAD_RSP);
        return true;
    }

    // 预读取 blob 数据
    std::vector<uint8_t> image_data;
    bool data_ok = (rec->size > 0) &&
                   ImageStorage::Instance().ReadRange(req.image_id(), 0, rec->size, image_data);

    if (!data_ok)
    {
        rsp.set_error(ERR_IMAGE_EXPIRED);
        rsp.set_offset(0);
        spdlog::info("[ImageService] HandleImageDownloadReq: {} blob unavailable (size={})",
                     req.image_id(), rec->size);

        std::string data;
        if (!rsp.SerializeToString(&data))
        {
            spdlog::error("[ImageService] HandleImageDownloadReq: serialize failed (blob unavailable)");
            return true;
        }
        session.Send(data, MSG_IMAGE_DOWNLOAD_RSP);
        return true;
    }

    // 授权成功
    rsp.set_error(0);
    rsp.set_offset(0);

    std::string rsp_data;
    if (!rsp.SerializeToString(&rsp_data))
    {
        spdlog::error("[ImageService] HandleImageDownloadReq: serialize failed (authorized)");
        return true;
    }
    session.Send(rsp_data, MSG_IMAGE_DOWNLOAD_RSP);
    spdlog::info("[ImageService] HandleImageDownloadReq: {} authorized, size={} ext={}",
                 req.image_id(), rec->size, rec->ext);

    // === 服务端主动推送文件（FileReq + FileChunk + FileAck）===
    int64_t task_id = static_cast<int64_t>(std::hash<std::string>{}(req.image_id()) & 0x7FFFFFFFFFFFFFFFLL);
    std::string filename = req.image_id() + "." + rec->ext;
    int requester_uid = session.GetUserUid();

    // 注册 FileTransfer 任务
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
        if (!fileReq.SerializeToString(&req_ser))
        {
            spdlog::error("[ImageService] HandleImageDownloadReq: FileReq serialize failed");
            return true;
        }
        session.Send(req_ser, MSG_FILE_REQ);
    }

    // 2. 分块发送 FileChunk
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
            if (!chunk.SerializeToString(&chunk_ser))
            {
                spdlog::error("[ImageService] HandleImageDownloadReq: FileChunk serialize failed at offset={}", offset);
                return true;
            }
            session.Send(chunk_ser, MSG_FILE_CHUNK);

            offset += this_chunk;
            remaining -= this_chunk;
        }
        spdlog::info("[ImageService] HandleImageDownloadReq: pushed {} bytes in {} chunks",
                     image_data.size(), (image_data.size() + kChunkSize - 1) / kChunkSize);
    }

    // 3. 发送 FileAck（完成通知）
    qmsrchat::FileAck fileAck;
    fileAck.set_task_id(task_id);
    fileAck.set_error(0);
    fileAck.set_message("complete");
    {
        std::string ack_ser;
        if (!fileAck.SerializeToString(&ack_ser))
        {
            spdlog::error("[ImageService] HandleImageDownloadReq: FileAck serialize failed");
            return true;
        }
        session.Send(ack_ser, MSG_FILE_ACK);
    }

    return true;
}

// ─── HandleChatRecall (MSG_CHAT_RECALL 1011) ───

bool ImageService::HandleChatRecall(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        qmsrchat::RecallMsg req;
        if (!req.ParseFromString(body_data))
        {
            spdlog::warn("HandleChatRecall: parse failed");
            return true;
        }
        const int from = session.GetUserUid();
        const int64_t now_ms = NowMs();

        // Phase 5E: 防重放 Nonce 校验
        if (req.has_nonce_header()) {
            const auto &nh = req.nonce_header();
            if (!NonceCache::Instance().IsWithinTimeWindow(nh.timestamp())) {
                spdlog::warn("HandleChatRecall: nonce time window exceeded");
                return true;
            }
            if (!NonceCache::Instance().TryInsert(nh.nonce())) {
                spdlog::warn("HandleChatRecall: duplicate nonce");
                return true;
            }
        }

        // 1. 查原消息（用 timestamp + from_uid 精确查找）
        auto orig = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(req.msg_timestamp(), from);
        if (!orig.has_value())
        {
            spdlog::warn("HandleChatRecall: msg not found or not owner, ts={} from={}",
                         req.msg_timestamp(), from);
            SendEditAck(session, MSG_CHAT_RECALL, ERR_RECALL_NOT_OWNER, req.msg_timestamp());
            return true;
        }

        // 2. 2 分钟窗口校验
        if (now_ms - req.msg_timestamp() > 2LL * 60 * 1000)
        {
            spdlog::warn("HandleChatRecall: timeout, age_ms={}", now_ms - req.msg_timestamp());
            SendEditAck(session, MSG_CHAT_RECALL, ERR_RECALL_TIMEOUT, req.msg_timestamp());
            return true;
        }

        // 3. 已撤回检查
        if (orig->recalled)
        {
            spdlog::warn("HandleChatRecall: already recalled, ts={}", req.msg_timestamp());
            SendEditAck(session, MSG_CHAT_RECALL, ERR_MSG_ALREADY_RECALLED, req.msg_timestamp());
            return true;
        }

        // 4. 联动 ImageStorage（如图片）
        if (orig->type == 1 && !orig->image_id.empty())
        {
            ImageStorage::Instance().MarkRecalled(orig->image_id);
            spdlog::info("HandleChatRecall: marked image_storage recalled, image_id={}", orig->image_id);
        }

        // 5. DB 标记 recalled
        if (!SQLiteMgr::Instance().Messages().MarkMessageRecalled(req.msg_timestamp(), from, now_ms))
        {
            spdlog::error("HandleChatRecall: DB mark failed, ts={}", req.msg_timestamp());
            SendEditAck(session, MSG_CHAT_RECALL, 1, req.msg_timestamp());
            return true;
        }

        // 6. 入 RecallNotifyQueue 兜底
        SQLiteMgr::Instance().Messages().EnqueueRecallNotify(orig->to_uid, req.msg_timestamp(), from, now_ms, orig->to_uid);
        spdlog::info("HandleChatRecall: Notify queued for uid={} ts={}", orig->to_uid, req.msg_timestamp());

        // 7. 尝试在线推送
        PushRecallNotify(orig->to_uid, from, req.msg_timestamp(), now_ms);

        // 8. 回 RecallAck 给发起方
        SendEditAck(session, MSG_CHAT_RECALL, 0, req.msg_timestamp(), now_ms, "", "ok");
    }
    catch (const std::exception &e)
    {
        spdlog::error("[ImageService] HandleChatRecall error: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("[ImageService] HandleChatRecall unknown exception");
    }
    return true;
}

// ─── HandleChatEdit (MSG_CHAT_EDIT 1012) ───

bool ImageService::HandleChatEdit(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        qmsrchat::EditMsg req;
        if (!req.ParseFromString(body_data))
        {
            spdlog::warn("HandleChatEdit: parse failed");
            return true;
        }
        const int from = session.GetUserUid();
        const int64_t now_ms = NowMs();

        // Phase 5E: 防重放 Nonce 校验
        if (req.has_nonce_header()) {
            const auto &nh = req.nonce_header();
            if (!NonceCache::Instance().IsWithinTimeWindow(nh.timestamp())) {
                spdlog::warn("HandleChatEdit: nonce time window exceeded");
                return true;
            }
            if (!NonceCache::Instance().TryInsert(nh.nonce())) {
                spdlog::warn("HandleChatEdit: duplicate nonce");
                return true;
            }
        }

        // 1. 长度校验
        if (req.new_content().size() > 2000)
        {
            SendEditAck(session, MSG_CHAT_EDIT, ERR_EDIT_TOO_LONG, req.msg_timestamp());
            return true;
        }

        // 2. 查原消息
        auto orig = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(req.msg_timestamp(), from);
        if (!orig.has_value())
        {
            spdlog::warn("HandleChatEdit: msg not found or not owner, ts={}", req.msg_timestamp());
            SendEditAck(session, MSG_CHAT_EDIT, ERR_EDIT_NOT_OWNER, req.msg_timestamp());
            return true;
        }

        // 3. 2 分钟窗口校验
        if (now_ms - req.msg_timestamp() > 2LL * 60 * 1000)
        {
            SendEditAck(session, MSG_CHAT_EDIT, ERR_EDIT_TIMEOUT, req.msg_timestamp());
            return true;
        }

        // 4. DB 更新 content + edited + edited_at
        if (!SQLiteMgr::Instance().Messages().UpdateMessageContent(req.msg_timestamp(), from,
                                                         req.new_content(), now_ms))
        {
            spdlog::error("HandleChatEdit: DB update failed, ts={}", req.msg_timestamp());
            SendEditAck(session, MSG_CHAT_EDIT, 1, req.msg_timestamp());
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
            if (!n.SerializeToString(&s))
            {
                spdlog::error("[ImageService] HandleChatEdit: EditNotify serialize failed");
                return true;
            }
            if (!MessageRouter::Instance().SendToSession(target_session, s, MSG_CHAT_EDIT_NOTIFY))
            {
                spdlog::warn("HandleChatEdit: Notify send failed");
            }
        }
        else
        {
            // 离线：入队编辑通知，上线时投递
            SQLiteMgr::Instance().Messages().EnqueueEditNotify(
                orig->to_uid, req.msg_timestamp(), from, req.new_content(), now_ms);
            spdlog::info("HandleChatEdit: target uid={} offline, Notify queued", orig->to_uid);
        }

        // 6. 回 EditAck 给发起方
        SendEditAck(session, MSG_CHAT_EDIT, 0, req.msg_timestamp(), now_ms, req.new_content());
    }
    catch (const std::exception &e)
    {
        spdlog::error("[ImageService] HandleChatEdit error: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("[ImageService] HandleChatEdit unknown exception");
    }
    return true;
}

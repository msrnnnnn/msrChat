/**
 * @file    FileService.cpp
 * @brief   文件传输相关消息处理器实现
 * @details 文件请求、响应、分片、确认
 */

#include "services/FileService.h"
#include "services/DispatchGuard.h"
#include "services/ImageService.h"
#include "CServer.h"
#include "CSession.h"
#include "FileTransfer.h"
#include "ImageStorage.h"
#include "Message.pb.h"
#include <spdlog/spdlog.h>
#include <ctime>

// ─── HandleFileReq (MSG_FILE_REQ 2001) ───

bool FileService::HandleFileReq(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    if (body_data.size() > MAX_FILE_META_SIZE)
    {
        spdlog::warn("[FileService] HandleFileReq: body too large {} > {}",
                     body_data.size(), MAX_FILE_META_SIZE);
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
        return true;
    }

    try
    {
        qmsrchat::FileReq fileReq;
        if (!fileReq.ParseFromString(body_data))
        {
            spdlog::error("[FileService] Failed to parse FileReq from Protobuf");
            return true;
        }

        int64_t task_id = fileReq.task_id();
        int to_uid = fileReq.to_uid();
        std::string filename = fileReq.filename();
        int64_t total_size = fileReq.total_size();

        if (fileReq.from_uid() != session.GetUserUid())
        {
            spdlog::warn("[FileService] HandleFileReq: from_uid mismatch {} vs session {}",
                         fileReq.from_uid(), session.GetUserUid());
            qmsrchat::FileAck response;
            response.set_error(1);
            response.set_message("uid mismatch");
            std::string serialized;
            if (response.SerializeToString(&serialized))
                session.Send(serialized, MSG_FILE_ACK);
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
            return true;
        }

        // 记录 P2P 路由映射，用于后续 chunk/ack 转发
        FileTransfer::Instance().AddTask(
            task_id, session.GetUserUid(), to_uid, filename, total_size);

        // ===== 图片模式检测 =====
        // 客户端以 "{uuid}.{ext}" 格式传 filename，image_id 即 UUID 部分
        // 匹配 UUID 格式后，预插入 ImageStorage 记录
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
                    spdlog::info("[FileService] HandleFileReq: target uid={} offline, "
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
    }
    catch (const std::exception &e)
    {
        spdlog::error("[FileService] HandleFileReq error: {}", e.what());
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("parse error");

        std::string serialized;
        if (response.SerializeToString(&serialized))
        {
            session.Send(serialized, MSG_FILE_ACK);
        }
    }
    catch (...)
    {
        spdlog::error("[FileService] HandleFileReq unknown exception");
    }
    return true;
}

// ─── HandleFileRsp (MSG_FILE_RSP 2002) ───

bool FileService::HandleFileRsp(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    try
    {
        qmsrchat::FileRsp fileRsp;
        if (!fileRsp.ParseFromString(body_data))
        {
            spdlog::error("[FileService] Failed to parse FileRsp from Protobuf");
            return true;
        }

        int64_t task_id = fileRsp.task_id();
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (!task)
        {
            spdlog::warn("[FileService] FileRsp: task {} not found", task_id);
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
            spdlog::info("[FileService] FileRsp forwarded: task_id={}, from_uid={}", task_id, from_uid);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[FileService] HandleFileRsp error: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("[FileService] HandleFileRsp unknown exception");
    }
    return true;
}

// ─── HandleFileChunk string 重载 (MSG_FILE_CHUNK 2003) ───

bool FileService::HandleFileChunk(CSession &session, const std::string &body_data)
{
    return HandleFileChunk(session, std::string_view(body_data));
}

// ─── HandleFileChunk string_view 重载 (MSG_FILE_CHUNK 2003) ───

bool FileService::HandleFileChunk(CSession &session, std::string_view body_view)
{
    DispatchGuard guard(session);

    if (session.GetUserUid() <= 0)
    {
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("not login");
        std::string serialized;
        if (response.SerializeToString(&serialized))
            session.Send(serialized, MSG_FILE_ACK);
        return true;
    }

    try
    {
        qmsrchat::FileChunk chunk;
        if (!chunk.ParseFromArray(body_view.data(), static_cast<int>(body_view.size())))
        {
            spdlog::error("[FileService] Failed to parse FileChunk");
            return true;
        }

        int64_t task_id = chunk.task_id();
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (!task)
        {
            spdlog::warn("[FileService] FileChunk: task {} not found", task_id);
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

            // 目标离线时，服务端代回 FileAck 给发送方
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
                        spdlog::info("[FileService] image upload complete (offline target): {}", task->GetImageId());
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

        spdlog::debug("[FileService] FileChunk forwarded: task_id={}, to_uid={}", task_id, to_uid);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[FileService] HandleFileChunk error: {}", e.what());
        qmsrchat::FileAck response;
        response.set_error(1);
        response.set_message("chunk processing error");
        std::string serialized;
        if (response.SerializeToString(&serialized))
            session.Send(serialized, MSG_FILE_ACK);
    }
    catch (...)
    {
        spdlog::error("[FileService] HandleFileChunk unknown exception");
    }
    return true;
}

// ─── HandleFileAck (MSG_FILE_ACK 2004) ───

bool FileService::HandleFileAck(CSession &session, const std::string &body_data)
{
    DispatchGuard guard(session);

    if (session.GetUserUid() <= 0)
    {
        return true;
    }

    try
    {
        qmsrchat::FileAck fileAck;
        if (!fileAck.ParseFromString(body_data))
        {
            spdlog::error("[FileService] Failed to parse FileAck from Protobuf");
            return true;
        }

        int64_t task_id = fileAck.task_id();
        auto task = FileTransfer::Instance().GetTask(task_id);
        if (!task)
        {
            spdlog::warn("[FileService] FileAck: task {} not found", task_id);
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
            if (task->IsImage())
            {
                ImageStorage::Instance().MarkCompleted(task->GetImageId());
                spdlog::info("[FileService] image upload complete: {}", task->GetImageId());
            }
            FileTransfer::Instance().RemoveTask(task_id);
            spdlog::info("[FileService] File transfer completed (received={}, total={}), task_id={} removed",
                         fileAck.received(), task->GetTotalSize(), task_id);
        }
        else if (task->IsImage() && task->IsTargetOffline())
        {
            ImageService::ContinueImageDownload(task_id);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[FileService] HandleFileAck error: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("[FileService] HandleFileAck unknown exception");
    }
    return true;
}

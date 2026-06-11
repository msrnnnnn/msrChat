/**
 * @file MessageRepository.cpp
 * @brief 消息数据仓库实现
 * @details 从 SQLiteMgr.cpp 拆分（Phase 5D），包含消息 CRUD、离线消息、撤回通知队列。
 */
#include "MessageRepository.h"
#include "const.h"
#include <climits>
#include <spdlog/spdlog.h>

// ============================================================
// 辅助函数（文件内部）
// ============================================================

static void ReadPhase7Columns(sqlite3_stmt *stmt, ChatMessage &msg)
{
    msg.type = sqlite3_column_int(stmt, 6);
    const unsigned char *iid = sqlite3_column_text(stmt, 7);
    if (iid) msg.image_id = std::string(reinterpret_cast<const char *>(iid));
    msg.recalled = sqlite3_column_int(stmt, 8) != 0;
    msg.recalled_at = sqlite3_column_int64(stmt, 9);
    msg.edited = sqlite3_column_int(stmt, 10) != 0;
    msg.edited_at = sqlite3_column_int64(stmt, 11);
}

// ============================================================
// MessageRepository 实现
// ============================================================

MessageRepository::MessageRepository(std::shared_ptr<SQLiteConnectionPool> pool)
    : _pool(std::move(pool))
{
}

bool MessageRepository::SaveMessage(const ChatMessage &msg)
{
    if (msg.from_uid <= 0 || msg.to_uid <= 0)
    {
        spdlog::warn("[MessageRepository] SaveMessage rejected: invalid uid from={} to={}", msg.from_uid, msg.to_uid);
        return false;
    }

    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "INSERT INTO messages (from_uid, to_uid, content, timestamp, status, "
        "client_msg_id, type, image_id, recalled, recalled_at, edited, edited_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);
    sqlite3_bind_text(stmt, 6, msg.client_msg_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, msg.type);
    sqlite3_bind_text(stmt, 8, msg.image_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, msg.recalled ? 1 : 0);
    sqlite3_bind_int64(stmt, 10, msg.recalled_at);
    sqlite3_bind_int(stmt, 11, msg.edited ? 1 : 0);
    sqlite3_bind_int64(stmt, 12, msg.edited_at);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::vector<ChatMessage> MessageRepository::GetMessages(int uid1, int uid2, int64_t before_time, int limit)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return {};
    }
    sqlite3 *db = guard.Get();

    std::vector<ChatMessage> messages;
    ScopedStmt stmt(db, R"(
        SELECT id, from_uid, to_uid, content, timestamp, status,
               type, image_id, recalled, recalled_at, edited, edited_at
        FROM messages
        WHERE ((from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?))
        AND timestamp < ?
        ORDER BY timestamp DESC
        LIMIT ?
    )");
    if (!stmt)
    {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, uid1);
    sqlite3_bind_int(stmt, 2, uid2);
    sqlite3_bind_int(stmt, 3, uid2);
    sqlite3_bind_int(stmt, 4, uid1);
    sqlite3_bind_int64(stmt, 5, before_time);
    sqlite3_bind_int(stmt, 6, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = SafeColumnText(stmt, 3);
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        ReadPhase7Columns(stmt, msg);
        messages.push_back(msg);
    }

    return messages;
}

std::optional<ChatMessage> MessageRepository::GetMessageByTimestamp(int64_t timestamp, int from_uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return std::nullopt;
    sqlite3 *db = guard.Get();

    const char *sql = R"SQL(
        SELECT id, from_uid, to_uid, content, timestamp, status,
               type, image_id, recalled, recalled_at, edited, edited_at
        FROM messages
        WHERE timestamp = ? AND from_uid = ?
        LIMIT 1
    )SQL";

    ScopedStmt stmt(db, sql);
    if (!stmt) return std::nullopt;
    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_int(stmt, 2, from_uid);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        msg.content = SafeColumnText(stmt, 3);
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        msg.type = sqlite3_column_int(stmt, 6);
        const unsigned char *iid = sqlite3_column_text(stmt, 7);
        if (iid) msg.image_id = std::string(reinterpret_cast<const char *>(iid));
        msg.recalled = sqlite3_column_int(stmt, 8) != 0;
        msg.recalled_at = sqlite3_column_int64(stmt, 9);
        msg.edited = sqlite3_column_int(stmt, 10) != 0;
        msg.edited_at = sqlite3_column_int64(stmt, 11);
        return msg;
    }
    return std::nullopt;
}

bool MessageRepository::MarkMessageRecalled(int64_t timestamp, int from_uid, int64_t recall_ts)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "UPDATE messages SET recalled = 1, recalled_at = ? WHERE timestamp = ? AND from_uid = ?");
    if (!stmt) return false;
    sqlite3_bind_int64(stmt, 1, recall_ts);
    sqlite3_bind_int64(stmt, 2, timestamp);
    sqlite3_bind_int(stmt, 3, from_uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool MessageRepository::UpdateMessageContent(int64_t timestamp, int from_uid,
                                              const std::string &new_content, int64_t edit_ts)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "UPDATE messages SET content = ?, edited = 1, edited_at = ? "
        "WHERE timestamp = ? AND from_uid = ?");
    if (!stmt) return false;
    sqlite3_bind_text(stmt, 1, new_content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, edit_ts);
    sqlite3_bind_int64(stmt, 3, timestamp);
    sqlite3_bind_int(stmt, 4, from_uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool MessageRepository::SaveOfflineMessage(const ChatMessage &msg)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(
        db, "INSERT INTO offline_messages (from_uid, to_uid, content, timestamp, status, client_msg_id, type, image_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.from_uid);
    sqlite3_bind_int(stmt, 2, msg.to_uid);
    if (msg.type == 1)
    {
        // 图片消息：content 存 Base64 编码的 protobuf binary，用 blob 绑定保留 \0 字节
        if (msg.content.size() > static_cast<size_t>(INT_MAX))
        {
            spdlog::error("[MessageRepository] SaveOfflineMessage: blob too large {} bytes", msg.content.size());
            return false;
        }
        sqlite3_bind_blob(stmt, 3, msg.content.data(), static_cast<int>(msg.content.size()), SQLITE_TRANSIENT);
    }
    else
    {
        // 文本消息：普通 bind_text
        sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_int(stmt, 5, msg.status);
    sqlite3_bind_text(stmt, 6, msg.client_msg_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, msg.type);
    sqlite3_bind_text(stmt, 8, msg.image_id.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::vector<ChatMessage> MessageRepository::GetOfflineMessages(int uid, int limit, int64_t after_id)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return {};
    }
    sqlite3 *db = guard.Get();

    std::vector<ChatMessage> messages;
    ScopedStmt stmt(db, R"(
        SELECT om.id, om.from_uid, om.to_uid, om.content, om.timestamp, om.status, om.client_msg_id, om.type, om.image_id
        FROM offline_messages om
        LEFT JOIN messages m ON m.timestamp = om.timestamp AND m.from_uid = om.from_uid
        WHERE om.to_uid = ? AND om.id > ? AND (m.recalled IS NULL OR m.recalled = 0)
        ORDER BY om.id ASC
        LIMIT ?
    )");
    if (!stmt)
    {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int64(stmt, 2, after_id);
    sqlite3_bind_int(stmt, 3, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.from_uid = sqlite3_column_int(stmt, 1);
        msg.to_uid = sqlite3_column_int(stmt, 2);
        // content: type=1（图片）时用 blob API 读取（保留 \0 字节），type=0（文本）用 text API
        msg.type = sqlite3_column_int(stmt, 7);
        if (msg.type == 1)
        {
            const void *blob = sqlite3_column_blob(stmt, 3);
            int blob_size = sqlite3_column_bytes(stmt, 3);
            if (blob && blob_size > 0)
            {
                msg.content = std::string(static_cast<const char *>(blob), blob_size);
            }
        }
        else
        {
            const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            msg.content = text ? std::string(text) : "";
        }
        msg.timestamp = sqlite3_column_int64(stmt, 4);
        msg.status = sqlite3_column_int(stmt, 5);
        const char *client_msg_id_text =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        msg.client_msg_id = client_msg_id_text ? std::string(client_msg_id_text) : "";
        const char *image_id_text =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        msg.image_id = image_id_text ? std::string(image_id_text) : "";
        messages.push_back(msg);
    }

    return messages;
}

int64_t MessageRepository::GetOfflineMessageCount(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return 0;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, R"(
        SELECT COUNT(*) FROM offline_messages om
        LEFT JOIN messages m ON m.timestamp = om.timestamp AND m.from_uid = om.from_uid
        WHERE om.to_uid = ? AND (m.recalled IS NULL OR m.recalled = 0)
    )");
    if (!stmt)
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, uid);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        return sqlite3_column_int64(stmt, 0);
    }

    return 0;
}

bool MessageRepository::ClearOfflineMessages(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard)
    {
        return false;
    }
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "DELETE FROM offline_messages WHERE to_uid = ?");
    if (!stmt)
    {
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool MessageRepository::EnqueueRecallNotify(int uid, int64_t msg_timestamp, int recall_uid,
                                             int64_t recall_ts, int recalled_to)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "INSERT INTO recall_notify_queue (uid, msg_timestamp, recall_uid, recall_ts, recalled_to) "
        "VALUES (?, ?, ?, ?, ?)");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int64(stmt, 2, msg_timestamp);
    sqlite3_bind_int(stmt, 3, recall_uid);
    sqlite3_bind_int64(stmt, 4, recall_ts);
    sqlite3_bind_int(stmt, 5, recalled_to);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::vector<RecallNotifyEntry> MessageRepository::PopRecallNotifies(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return {};
    sqlite3 *db = guard.Get();

    std::vector<RecallNotifyEntry> entries;
    ScopedStmt stmt(db,
        "SELECT id, uid, msg_timestamp, recall_uid, recall_ts, recalled_to "
        "FROM recall_notify_queue WHERE uid = ? "
        "ORDER BY id ASC");
    if (!stmt) return entries;
    sqlite3_bind_int(stmt, 1, uid);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        RecallNotifyEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.uid = sqlite3_column_int(stmt, 1);
        e.msg_timestamp = sqlite3_column_int64(stmt, 2);
        e.recall_uid = sqlite3_column_int(stmt, 3);
        e.recall_ts = sqlite3_column_int64(stmt, 4);
        e.recalled_to = sqlite3_column_int(stmt, 5);
        entries.push_back(e);
    }
    return entries;
}

bool MessageRepository::ClearRecallNotifies(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "DELETE FROM recall_notify_queue WHERE uid = ?");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool MessageRepository::ClearRecallNotifyByTimestamp(int uid, int64_t msg_timestamp)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "DELETE FROM recall_notify_queue WHERE uid = ? AND msg_timestamp = ?");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int64(stmt, 2, msg_timestamp);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

// ============================================================
// 编辑通知队列
// ============================================================

bool MessageRepository::EnqueueEditNotify(int uid, int64_t msg_timestamp, int from_uid,
                                          const std::string &new_content, int64_t edit_ts)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db,
        "INSERT INTO edit_notify_queue (uid, msg_timestamp, from_uid, new_content, edit_ts) "
        "VALUES (?, ?, ?, ?, ?)");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int64(stmt, 2, msg_timestamp);
    sqlite3_bind_int(stmt, 3, from_uid);
    sqlite3_bind_text(stmt, 4, new_content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, edit_ts);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::vector<EditNotifyEntry> MessageRepository::PopEditNotifies(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return {};
    sqlite3 *db = guard.Get();

    std::vector<EditNotifyEntry> entries;
    ScopedStmt stmt(db,
        "SELECT id, uid, msg_timestamp, from_uid, new_content, edit_ts "
        "FROM edit_notify_queue WHERE uid = ? "
        "ORDER BY id ASC");
    if (!stmt) return entries;
    sqlite3_bind_int(stmt, 1, uid);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EditNotifyEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.uid = sqlite3_column_int(stmt, 1);
        e.msg_timestamp = sqlite3_column_int64(stmt, 2);
        e.from_uid = sqlite3_column_int(stmt, 3);
        e.new_content = SafeColumnText(stmt, 4);
        e.edit_ts = sqlite3_column_int64(stmt, 5);
        entries.push_back(e);
    }
    return entries;
}

bool MessageRepository::ClearEditNotifies(int uid)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    ScopedStmt stmt(db, "DELETE FROM edit_notify_queue WHERE uid = ?");
    if (!stmt) return false;
    sqlite3_bind_int(stmt, 1, uid);

    return sqlite3_step(stmt) == SQLITE_DONE;
}

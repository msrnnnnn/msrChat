#pragma once
/**
 * @file MessageRepository.h
 * @brief 消息数据仓库 —— 消息 CRUD、离线消息、撤回通知队列
 * @details 从 SQLiteMgr 拆分（Phase 5D），负责所有消息相关的数据库操作。
 */
#ifndef MESSAGE_REPOSITORY_H
#define MESSAGE_REPOSITORY_H

#include "SQLiteMgr.h"  // SQLiteConnectionPool, SQLiteConnectionGuard, ScopedStmt, ChatMessage, RecallNotifyEntry
#include <optional>
#include <string>
#include <vector>

class MessageRepository
{
public:
    explicit MessageRepository(std::shared_ptr<SQLiteConnectionPool> pool);

    // === 消息 CRUD ===
    bool SaveMessage(const ChatMessage &msg);
    bool MessageExists(const std::string &client_msg_id);
    std::vector<ChatMessage> GetMessages(int uid1, int uid2,
                                          int64_t before_time, int limit = 50);
    std::optional<ChatMessage> GetMessageByTimestamp(int64_t timestamp, int from_uid);
    bool MarkMessageRecalled(int64_t timestamp, int from_uid, int64_t recall_ts);
    bool UpdateMessageContent(int64_t timestamp, int from_uid,
                               const std::string &new_content, int64_t edit_ts);

    // === 离线消息 ===
    bool SaveOfflineMessage(const ChatMessage &msg);
    std::vector<ChatMessage> GetOfflineMessages(int uid, int limit,
                                                 int64_t after_id = 0);
    int64_t GetOfflineMessageCount(int uid);
    bool ClearOfflineMessages(int uid);

    // === 撤回通知队列 ===
    bool EnqueueRecallNotify(int uid, int64_t msg_timestamp,
                              int recall_uid, int64_t recall_ts, int recalled_to);
    std::vector<RecallNotifyEntry> PopRecallNotifies(int uid);
    bool ClearRecallNotifies(int uid);
    bool ClearRecallNotifyByTimestamp(int uid, int64_t msg_timestamp);

    // === 编辑通知队列 ===
    bool EnqueueEditNotify(int uid, int64_t msg_timestamp, int from_uid,
                           const std::string &new_content, int64_t edit_ts);
    std::vector<EditNotifyEntry> PopEditNotifies(int uid);
    bool ClearEditNotifies(int uid);

private:
    std::shared_ptr<SQLiteConnectionPool> _pool;
};

#endif // MESSAGE_REPOSITORY_H

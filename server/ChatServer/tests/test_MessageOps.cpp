/**
 * @file test_MessageOps.cpp
 * @brief Phase 4.5 T.2 — 核心消息操作集成测试
 * @details 测试 SQLiteMgr 的消息 CRUD、撤回/编辑、离线消息、撤回通知队列。
 *          使用手动时间戳确保测试确定性，不依赖系统时钟。
 */
#include "SQLiteMgr.h"
#include "MessageRepository.h"
#include "const.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

class MessageOpsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _db_path = "test_msg_ops_" + std::to_string(std::time(nullptr)) + "_" +
                   std::to_string(std::rand()) + ".db";
        ASSERT_TRUE(SQLiteMgr::Instance().Init(_db_path, 2));
    }

    void TearDown() override
    {
        SQLiteMgr::Instance().Shutdown();
        std::remove(_db_path.c_str());
    }

    /// 构造一条基础 ChatMessage，时间戳由调用者指定
    ChatMessage MakeMsg(int from, int to, const std::string &content, int64_t ts)
    {
        ChatMessage msg;
        msg.from_uid = from;
        msg.to_uid = to;
        msg.content = content;
        msg.timestamp = ts;
        msg.status = 1;
        msg.type = 0;
        return msg;
    }

    std::string _db_path;
};

// ============================================================
// 消息存取
// ============================================================

TEST_F(MessageOpsTest, SaveAndGetMessages)
{
    auto msg1 = MakeMsg(1, 2, "hello", 1700000001000LL);
    auto msg2 = MakeMsg(2, 1, "world", 1700000002000LL);
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg1));
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg2));

    // before_time 大于所有消息时间戳，limit 足够
    auto results = SQLiteMgr::Instance().Messages().GetMessages(1, 2, 1700000099000LL, 50);
    ASSERT_EQ(results.size(), 2u);
    // GetMessages 按 timestamp DESC 排列
    EXPECT_EQ(results[0].content, "world");
    EXPECT_EQ(results[1].content, "hello");
}

TEST_F(MessageOpsTest, GetMessages_Pagination)
{
    for (int i = 0; i < 5; i++)
    {
        auto msg = MakeMsg(10, 20, "msg_" + std::to_string(i), 1700000000000LL + i * 1000);
        ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg));
    }

    auto results = SQLiteMgr::Instance().Messages().GetMessages(10, 20, 1700000099000LL, 3);
    EXPECT_EQ(results.size(), 3u);
    // 最新 3 条（DESC 序）
    EXPECT_EQ(results[0].content, "msg_4");
    EXPECT_EQ(results[1].content, "msg_3");
    EXPECT_EQ(results[2].content, "msg_2");
}

TEST_F(MessageOpsTest, GetMessages_BeforeTime)
{
    auto msg1 = MakeMsg(30, 40, "early", 1000LL);
    auto msg2 = MakeMsg(30, 40, "mid", 2000LL);
    auto msg3 = MakeMsg(30, 40, "late", 3000LL);
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg1));
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg2));
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg3));

    // before_time = 2500，应只返回 ts < 2500 的消息
    auto results = SQLiteMgr::Instance().Messages().GetMessages(30, 40, 2500LL, 50);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].content, "mid");
    EXPECT_EQ(results[1].content, "early");
}

TEST_F(MessageOpsTest, SaveMessage_InvalidUid)
{
    auto msg = MakeMsg(0, 1, "bad", 1000LL);
    EXPECT_FALSE(SQLiteMgr::Instance().Messages().SaveMessage(msg));

    auto msg2 = MakeMsg(1, -1, "bad2", 2000LL);
    EXPECT_FALSE(SQLiteMgr::Instance().Messages().SaveMessage(msg2));
}

// ============================================================
// 撤回 / 编辑
// ============================================================

TEST_F(MessageOpsTest, Recall_MarkAndVerify)
{
    auto msg = MakeMsg(100, 200, "recall me", 1700000010000LL);
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg));

    int64_t recall_ts = 1700000020000LL;
    SQLiteMgr::Instance().MarkMessageRecalled(1700000010000LL, 100, recall_ts);

    auto got = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(1700000010000LL, 100);
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(got->recalled);
    EXPECT_EQ(got->recalled_at, recall_ts);
}

TEST_F(MessageOpsTest, Edit_UpdateContent)
{
    auto msg = MakeMsg(100, 200, "original", 1700000030000LL);
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveMessage(msg));

    int64_t edit_ts = 1700000040000LL;
    SQLiteMgr::Instance().UpdateMessageContent(1700000030000LL, 100, "edited content", edit_ts);

    auto got = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(1700000030000LL, 100);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->content, "edited content");
    EXPECT_TRUE(got->edited);
    EXPECT_EQ(got->edited_at, edit_ts);
}

TEST_F(MessageOpsTest, Recall_NonexistentMsg)
{
    // 对不存在的消息执行 MarkMessageRecalled — sqlite3_step 返回 SQLITE_DONE（0 rows affected）
    // 方法返回 true（SQL 执行成功），但 GetMessageByTimestamp 返回 nullopt
    bool sql_ok = SQLiteMgr::Instance().MarkMessageRecalled(9999999999LL, 999, 1700000000000LL);
    EXPECT_TRUE(sql_ok); // SQL 执行成功（无匹配行不影响 DONE 状态）
    auto got = SQLiteMgr::Instance().Messages().GetMessageByTimestamp(9999999999LL, 999);
    EXPECT_FALSE(got.has_value());
}

// ============================================================
// 离线消息
// ============================================================

TEST_F(MessageOpsTest, OfflineMessages)
{
    ChatMessage off1 = MakeMsg(50, 60, "offline_1", 1700000050000LL);
    ChatMessage off2 = MakeMsg(50, 60, "offline_2", 1700000051000LL);
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveOfflineMessage(off1));
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().SaveOfflineMessage(off2));

    auto msgs = SQLiteMgr::Instance().Messages().GetOfflineMessages(60, 50);
    ASSERT_EQ(msgs.size(), 2u);
    EXPECT_EQ(msgs[0].content, "offline_1");
    EXPECT_EQ(msgs[1].content, "offline_2");

    // 清空后为空
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().ClearOfflineMessages(60));
    auto after_clear = SQLiteMgr::Instance().Messages().GetOfflineMessages(60, 50);
    EXPECT_TRUE(after_clear.empty());
}

// ============================================================
// 撤回通知队列
// ============================================================

TEST_F(MessageOpsTest, RecallNotify_Flow)
{
    constexpr int target_uid = 70;
    constexpr int64_t msg_ts = 1700000060000LL;
    constexpr int recall_uid = 71;
    constexpr int64_t recall_ts = 1700000061000LL;
    constexpr int recalled_to = 70;

    ASSERT_TRUE(SQLiteMgr::Instance().Messages().EnqueueRecallNotify(target_uid, msg_ts, recall_uid, recall_ts, recalled_to));

    auto entries = SQLiteMgr::Instance().Messages().PopRecallNotifies(target_uid);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].uid, target_uid);
    EXPECT_EQ(entries[0].msg_timestamp, msg_ts);
    EXPECT_EQ(entries[0].recall_uid, recall_uid);
    EXPECT_EQ(entries[0].recall_ts, recall_ts);
    EXPECT_EQ(entries[0].recalled_to, recalled_to);

    // 清空后 Pop 返回空
    ASSERT_TRUE(SQLiteMgr::Instance().Messages().ClearRecallNotifies(target_uid));
    auto after_clear = SQLiteMgr::Instance().Messages().PopRecallNotifies(target_uid);
    EXPECT_TRUE(after_clear.empty());
}

#include "ChatListModel.h"
#include "DbService.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <QDateTime>

TEST(ChatListModelTest, AddImageMessageExposesRoles)
{
    ChatListModel m;
    m.SetCurrentUid(100);
    ChatMessage img;
    img.from_uid = 200;
    img.to_uid = 100;
    img.type = 1;
    img.image_id = "uuid-1";
    img.image_path = "/tmp/img1.jpg";
    img.image_width = 800;
    img.image_height = 600;
    img.timestamp = 1234567890;
    m.AddMessage(img);

    QModelIndex idx = m.index(0);
    EXPECT_EQ(m.data(idx, ChatListModel::TypeRole).toInt(), 1);
    EXPECT_EQ(m.data(idx, ChatListModel::ImageIdRole).toString().toStdString(), "uuid-1");
    EXPECT_EQ(m.data(idx, ChatListModel::ImagePathRole).toString().toStdString(), "/tmp/img1.jpg");
    EXPECT_EQ(m.data(idx, ChatListModel::ImageWidthRole).toInt(), 800);
    EXPECT_EQ(m.data(idx, ChatListModel::ImageHeightRole).toInt(), 600);
    EXPECT_FALSE(m.data(idx, ChatListModel::IsSelfRole).toBool());
}

TEST(ChatListModelTest, RecallFlagHidesContent)
{
    ChatListModel m;
    ChatMessage msg;
    msg.from_uid = 100;
    msg.to_uid = 200;
    msg.content = "old content";
    msg.recalled = true;
    m.AddMessage(msg);
    QModelIndex idx = m.index(0);
    EXPECT_TRUE(m.data(idx, ChatListModel::RecalledRole).toBool());
}

TEST(ChatListModelTest, ConcurrentAddMessage)
{
    ChatListModel m;
    m.SetCurrentUid(1);

    std::atomic<int> crash_count{0};
    const int thread_count = 8;
    const int msgs_per_thread = 100;

    auto worker = [&](int tid) {
        for (int i = 0; i < msgs_per_thread; ++i)
        {
            try
            {
                ChatMessage msg;
                msg.from_uid = tid;
                msg.to_uid = 2;
                msg.content = QString::fromStdString("msg_" + std::to_string(tid) + "_" + std::to_string(i));
                msg.timestamp = QDateTime::currentMSecsSinceEpoch() + i;
                m.AddMessage(msg);
            }
            catch (...)
            {
                crash_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t)
        threads.emplace_back(worker, t);
    for (auto &th : threads)
        th.join();

    EXPECT_EQ(crash_count.load(), 0);
    EXPECT_EQ(m.rowCount(), thread_count * msgs_per_thread);
}
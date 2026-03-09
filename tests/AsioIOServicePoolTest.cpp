#include "server/ChatServer/include/AsioIOServicePool.h"
#include <gtest/gtest.h>

TEST(AsioIOServicePoolTest, Constructor_DefaultThreadPoolSize)
{
    AsioIOServicePool pool;
    EXPECT_EQ(pool.getThreadPoolSize(), 1);
}

TEST(AsioIOServicePoolTest, SetThreadPoolSize)
{
    AsioIOServicePool pool;
    pool.setThreadPoolSize(4);
    EXPECT_EQ(pool.getThreadPoolSize(), 4);
}

TEST(AsioIOServicePoolTest, SetThreadPoolSize_Zero)
{
    AsioIOServicePool pool;
    pool.setThreadPoolSize(0);
    EXPECT_EQ(pool.getThreadPoolSize(), 1); // 最小线程数为1
}

TEST(AsioIOServicePoolTest, SetThreadPoolSize_Negative)
{
    AsioIOServicePool pool;
    pool.setThreadPoolSize(-1);
    EXPECT_EQ(pool.getThreadPoolSize(), 1); // 最小线程数为1
}

TEST(AsioIOServicePoolTest, SetThreadPoolSize_Large)
{
    AsioIOServicePool pool;
    pool.setThreadPoolSize(100);
    EXPECT_EQ(pool.getThreadPoolSize(), 100);
}

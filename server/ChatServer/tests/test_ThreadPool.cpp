#include <gtest/gtest.h>
#include "ThreadPool.h"

#include <atomic>
#include <chrono>

TEST(ThreadPoolTest, ConstructWithThreads)
{
    ThreadPool pool(4);
    pool.Shutdown();
}

TEST(ThreadPoolTest, EnqueueAndExecute)
{
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    pool.Enqueue([&] { counter.fetch_add(1); });
    pool.Enqueue([&] { counter.fetch_add(1); });
    pool.Enqueue([&] { counter.fetch_add(1); });

    pool.Shutdown();
    EXPECT_EQ(counter.load(), 3);
}

TEST(ThreadPoolTest, TaskCount)
{
    ThreadPool pool(1);
    std::atomic<bool> started{false};
    std::atomic<bool> done{false};

    pool.Enqueue(
        [&]
        {
            started.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            done.store(true);
        });

    while (!started.load())
    {
        std::this_thread::yield();
    }

    pool.Shutdown();
    EXPECT_TRUE(done.load());
}

TEST(ThreadPoolTest, Shutdown)
{
    ThreadPool pool(2);
    pool.Enqueue([&] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
    pool.Shutdown();
}

TEST(ThreadPoolTest, DefaultConstructor)
{
    ThreadPool pool;
    pool.Shutdown();
}

TEST(ThreadPoolTest, MultipleEnqueueBeforeShutdown)
{
    ThreadPool pool(4);
    std::atomic<int> sum{0};

    for (int i = 0; i < 100; ++i)
    {
        pool.Enqueue([&sum, i] { sum.fetch_add(i); });
    }

    pool.Shutdown();
    EXPECT_EQ(sum.load(), 4950);
}

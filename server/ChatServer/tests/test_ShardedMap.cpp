#include <gtest/gtest.h>
#include "ShardedMap.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

TEST(ShardedMapTest, Construct)
{
    ShardedMap<int, std::string> map(4);
    EXPECT_EQ(map.ShardCount(), 4);
    EXPECT_EQ(map.Size(), 0);
}

TEST(ShardedMapTest, InsertAndFind)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");
    EXPECT_EQ(map.Size(), 1);

    auto v = map.Find(1);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), "one");
}

TEST(ShardedMapTest, FindMissing)
{
    ShardedMap<int, std::string> map(4);
    EXPECT_FALSE(map.Find(99).has_value());
}

TEST(ShardedMapTest, ConstFind)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(42, "answer");

    const auto &cmap = map;
    auto v = cmap.Find(42);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), "answer");
}

TEST(ShardedMapTest, Erase)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");
    EXPECT_EQ(map.Size(), 1);

    map.Erase(1);
    EXPECT_EQ(map.Size(), 0);
    EXPECT_FALSE(map.Find(1).has_value());
}

TEST(ShardedMapTest, EraseMissingNoOp)
{
    ShardedMap<int, std::string> map(4);
    map.Erase(99);
    EXPECT_EQ(map.Size(), 0);
}

TEST(ShardedMapTest, RemoveIfMatch)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");
    map.Insert(2, "two");

    bool removed = map.RemoveIfMatch(1, [](const std::string &v) { return v == "one"; });
    EXPECT_TRUE(removed);
    EXPECT_FALSE(map.Find(1).has_value());
    EXPECT_TRUE(map.Find(2).has_value());
}

TEST(ShardedMapTest, RemoveIfMatchPredicateFails)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");

    bool removed = map.RemoveIfMatch(1, [](const std::string &v) { return v == "wrong"; });
    EXPECT_FALSE(removed);
    EXPECT_TRUE(map.Find(1).has_value());
    EXPECT_EQ(map.Size(), 1);
}

TEST(ShardedMapTest, RemoveIfMatchMissingKey)
{
    ShardedMap<int, std::string> map(4);
    bool removed = map.RemoveIfMatch(99, [](const std::string &) { return true; });
    EXPECT_FALSE(removed);
}

TEST(ShardedMapTest, Clear)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");
    map.Insert(2, "two");
    map.Insert(3, "three");
    EXPECT_EQ(map.Size(), 3);

    map.Clear();
    EXPECT_EQ(map.Size(), 0);
    EXPECT_FALSE(map.Find(1).has_value());
}

TEST(ShardedMapTest, ForEach)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");
    map.Insert(2, "two");

    std::vector<std::pair<int, std::string>> collected;
    map.ForEach([&](int k, const std::string &v) { collected.emplace_back(k, v); });

    EXPECT_EQ(collected.size(), 2);
}

TEST(ShardedMapTest, ConstForEach)
{
    ShardedMap<int, std::string> map(4);
    map.Insert(1, "one");

    const auto &cmap = map;
    std::vector<std::pair<int, std::string>> collected;
    cmap.ForEach([&](int k, const std::string &v) { collected.emplace_back(k, v); });

    EXPECT_EQ(collected.size(), 1);
}

TEST(ShardedMapTest, SingleShard)
{
    ShardedMap<int, std::string> map(1);
    map.Insert(1, "one");
    map.Insert(2, "two");
    EXPECT_EQ(map.Size(), 2);
    EXPECT_TRUE(map.Find(1).has_value());
    EXPECT_TRUE(map.Find(2).has_value());
}

TEST(ShardedMapTest, ConcurrentInsertFind)
{
    ShardedMap<int, std::string> map(32);
    std::atomic<int> errors{0};

    auto writer = [&](int start, int count)
    {
        for (int i = start; i < start + count; ++i)
        {
            map.Insert(i, "val_" + std::to_string(i));
        }
    };

    auto reader = [&](int start, int count)
    {
        for (int i = start; i < start + count; ++i)
        {
            auto v = map.Find(i);
            if (!v.has_value() && !map.Find(i).has_value())
            {
                errors.fetch_add(1);
            }
        }
    };

    std::thread t1(writer, 0, 500);
    std::thread t2(writer, 500, 500);
    std::thread t3(reader, 0, 1000);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(map.Size(), 1000);
}

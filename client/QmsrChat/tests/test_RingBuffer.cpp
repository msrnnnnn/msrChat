#include <gtest/gtest.h>
#include "RingBuffer.h"

#include <cstring>

TEST(RingBufferTest, DefaultCapacity)
{
    RingBuffer rb;
    EXPECT_EQ(rb.Capacity(), RingBuffer::kDefaultCapacity);
    EXPECT_EQ(rb.Available(), 0);
}

TEST(RingBufferTest, CustomCapacity)
{
    RingBuffer rb(1024);
    EXPECT_EQ(rb.Capacity(), 1024);
}

TEST(RingBufferTest, WriteReadRoundtrip)
{
    RingBuffer rb(1024);
    const char *msg = "hello world";
    std::size_t len = std::strlen(msg);

    ASSERT_TRUE(rb.Write(msg, len));
    EXPECT_EQ(rb.Available(), len);

    char buf[64] = {0};
    std::size_t n = rb.Read(buf, sizeof(buf));
    EXPECT_EQ(n, len);
    EXPECT_STREQ(buf, msg);
    EXPECT_EQ(rb.Available(), 0);
}

TEST(RingBufferTest, WriteZeroLen)
{
    RingBuffer rb(1024);
    EXPECT_TRUE(rb.Write(nullptr, 0));
    EXPECT_EQ(rb.Available(), 0);
}

TEST(RingBufferTest, ReadEmptyBuffer)
{
    RingBuffer rb(1024);
    char buf[64];
    EXPECT_EQ(rb.Read(buf, sizeof(buf)), 0);
}

TEST(RingBufferTest, ReadPartial)
{
    RingBuffer rb(1024);
    const char *msg = "abcdef";
    std::size_t len = std::strlen(msg);

    ASSERT_TRUE(rb.Write(msg, len));

    char buf[64] = {0};
    std::size_t n = rb.Read(buf, 3);
    EXPECT_EQ(n, 3);
    EXPECT_STREQ(buf, "abc");
    EXPECT_EQ(rb.Available(), 3);

    n = rb.Read(buf, sizeof(buf));
    EXPECT_EQ(n, 3);
    EXPECT_STREQ(buf, "def");
    EXPECT_EQ(rb.Available(), 0);
}

TEST(RingBufferTest, Peek)
{
    RingBuffer rb(1024);
    const char *msg = "test data";
    std::size_t len = std::strlen(msg);

    ASSERT_TRUE(rb.Write(msg, len));

    char buf[64] = {0};
    ASSERT_TRUE(rb.Peek(0, buf, 4));
    EXPECT_STREQ(buf, "test");

    EXPECT_EQ(rb.Available(), len);
}

TEST(RingBufferTest, PeekInsufficient)
{
    RingBuffer rb(1024);
    ASSERT_TRUE(rb.Write("abc", 3));

    char buf[64];
    EXPECT_FALSE(rb.Peek(0, buf, 10));
}

TEST(RingBufferTest, Consume)
{
    RingBuffer rb(1024);
    ASSERT_TRUE(rb.Write("1234567890", 10));

    rb.Consume(3);
    EXPECT_EQ(rb.Available(), 7);

    char buf[64] = {0};
    rb.Read(buf, 7);
    EXPECT_STREQ(buf, "4567890");
}

TEST(RingBufferTest, ConsumeMoreThanAvailable)
{
    RingBuffer rb(1024);
    ASSERT_TRUE(rb.Write("abc", 3));

    rb.Consume(100);
    EXPECT_EQ(rb.Available(), 0);
}

TEST(RingBufferTest, Clear)
{
    RingBuffer rb(1024);
    ASSERT_TRUE(rb.Write("data", 4));
    EXPECT_EQ(rb.Available(), 4);

    rb.Clear();
    EXPECT_EQ(rb.Available(), 0);

    ASSERT_TRUE(rb.Write("new", 3));
    EXPECT_EQ(rb.Available(), 3);
}

TEST(RingBufferTest, AutoExpand)
{
    RingBuffer rb(256);
    std::string data(500, 'X');

    ASSERT_TRUE(rb.Write(data.c_str(), data.size()));
    EXPECT_EQ(rb.Available(), 500);
    EXPECT_GE(rb.Capacity(), 500);
}

TEST(RingBufferTest, MultipleWriteReadCycles)
{
    RingBuffer rb(512);

    for (int i = 0; i < 100; ++i)
    {
        ASSERT_TRUE(rb.Write(reinterpret_cast<const char *>(&i), sizeof(i)));
    }

    EXPECT_EQ(rb.Available(), 100 * sizeof(int));

    for (int i = 0; i < 100; ++i)
    {
        int val;
        ASSERT_EQ(rb.Read(reinterpret_cast<char *>(&val), sizeof(val)), sizeof(int));
        EXPECT_EQ(val, i);
    }

    EXPECT_EQ(rb.Available(), 0);
}

TEST(RingBufferTest, ExceedMaxCapacity)
{
    RingBuffer rb(256);

    std::string data(RingBuffer::kMaxCapacity, 'X');
    EXPECT_FALSE(rb.Write(data.c_str(), data.size()));
}

TEST(RingBufferTest, MoveConstructor)
{
    RingBuffer rb(1024);
    ASSERT_TRUE(rb.Write("move", 4));

    RingBuffer rb2(std::move(rb));
    EXPECT_EQ(rb2.Available(), 4);
    EXPECT_EQ(rb.Capacity(), 0);

    char buf[8] = {0};
    rb2.Read(buf, 4);
    EXPECT_STREQ(buf, "move");
}

TEST(RingBufferTest, WrapAroundWriteRead)
{
    RingBuffer rb(64);
    std::string step1(40, 'A');
    std::string step2(30, 'B');

    ASSERT_TRUE(rb.Write(step1.c_str(), step1.size()));
    EXPECT_EQ(rb.Available(), 40);

    char buf[64] = {0};
    std::size_t n = rb.Read(buf, 20);
    EXPECT_EQ(n, 20);
    EXPECT_EQ(rb.Available(), 20);

    ASSERT_TRUE(rb.Write(step2.c_str(), step2.size()));
    EXPECT_EQ(rb.Available(), 50);

    char buf2[64] = {0};
    n = rb.Read(buf2, 50);
    EXPECT_EQ(n, 50);
    EXPECT_EQ(rb.Available(), 0);
}

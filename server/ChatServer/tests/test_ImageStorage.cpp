#include "ImageStorage.h"
#include "SQLiteMgr.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <ctime>

class ImageStorageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _db_path = "test_image_storage_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(rand()) + ".db";
        ASSERT_TRUE(SQLiteMgr::Instance().Init(_db_path, 2));
        ASSERT_TRUE(ImageStorage::Instance().Init(SQLiteMgr::Instance().GetPool()));
    }
    void TearDown() override
    {
        SQLiteMgr::Instance().Shutdown();
        std::remove(_db_path.c_str());
    }
    std::string _db_path;
};

TEST_F(ImageStorageTest, InsertAndGet)
{
    ImageRecord rec;
    rec.image_id = "test-img-1";
    rec.from_uid = 100;
    rec.to_uid = 200;
    rec.ext = "jpg";
    rec.size = 1024;
    rec.md5 = "abc123";
    rec.width = 800;
    rec.height = 600;
    rec.created_at = std::time(nullptr);
    rec.expires_at = rec.created_at + 7 * 24 * 3600;
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec));

    auto got = ImageStorage::Instance().Get("test-img-1");
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->from_uid, 100);
    EXPECT_EQ(got->to_uid, 200);
    EXPECT_EQ(got->size, 1024);
    EXPECT_EQ(got->ext, "jpg");
    EXPECT_FALSE(got->recalled);
}

TEST_F(ImageStorageTest, MarkRecalled)
{
    ImageRecord rec;
    rec.image_id = "test-img-2";
    rec.from_uid = 1;
    rec.to_uid = 2;
    rec.ext = "png";
    rec.size = 100;
    rec.md5 = "x";
    rec.width = 10;
    rec.height = 10;
    rec.created_at = std::time(nullptr);
    rec.expires_at = rec.created_at + 3600;
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec));
    ASSERT_TRUE(ImageStorage::Instance().MarkRecalled("test-img-2"));
    auto got = ImageStorage::Instance().Get("test-img-2");
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(got->recalled);
}

TEST_F(ImageStorageTest, DeleteExpired_RemovesAllExpired)
{
    // 过期且已撤回的图片 — 应被删除
    ImageRecord rec1;
    rec1.image_id = "old-recalled";
    rec1.from_uid = 1;
    rec1.to_uid = 2;
    rec1.ext = "png";
    rec1.size = 100;
    rec1.md5 = "x";
    rec1.width = 10;
    rec1.height = 10;
    rec1.created_at = 1000;
    rec1.expires_at = 2000; // 早就过期
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec1));
    ASSERT_TRUE(ImageStorage::Instance().MarkRecalled("old-recalled"));

    // 过期但未撤回的图片 — 也应被删除
    ImageRecord rec2;
    rec2.image_id = "old-not-recalled";
    rec2.from_uid = 1;
    rec2.to_uid = 2;
    rec2.ext = "png";
    rec2.size = 100;
    rec2.md5 = "y";
    rec2.width = 10;
    rec2.height = 10;
    rec2.created_at = 1000;
    rec2.expires_at = 2000; // 也过期了
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec2));

    // 未过期的图片 — 不应被删除
    ImageRecord rec3;
    rec3.image_id = "fresh";
    rec3.from_uid = 1;
    rec3.to_uid = 2;
    rec3.ext = "png";
    rec3.size = 100;
    rec3.md5 = "z";
    rec3.width = 10;
    rec3.height = 10;
    rec3.created_at = 1000;
    rec3.expires_at = 999999; // 未过期
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec3));

    int deleted = ImageStorage::Instance().DeleteExpired(3000);
    EXPECT_GE(deleted, 2);
    EXPECT_FALSE(ImageStorage::Instance().Get("old-recalled").has_value());
    EXPECT_FALSE(ImageStorage::Instance().Get("old-not-recalled").has_value());
    EXPECT_TRUE(ImageStorage::Instance().Get("fresh").has_value());
}

TEST_F(ImageStorageTest, AppendChunk_SequentialWrite)
{
    // 插入一条空 blob 的记录
    ImageRecord rec;
    rec.image_id = "chunk-seq-1";
    rec.from_uid = 10;
    rec.to_uid = 20;
    rec.ext = "bin";
    rec.size = 12;
    rec.md5 = "abc";
    rec.width = 1;
    rec.height = 1;
    rec.created_at = std::time(nullptr);
    rec.expires_at = rec.created_at + 3600;
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec));

    // 分 3 个 chunk 顺序写入，每个 4 字节
    uint8_t c1[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t c2[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t c3[] = {0xFF, 0xEE, 0xDD, 0xCC};
    ASSERT_TRUE(ImageStorage::Instance().AppendChunk("chunk-seq-1", 0, c1, 4));
    ASSERT_TRUE(ImageStorage::Instance().AppendChunk("chunk-seq-1", 4, c2, 4));
    ASSERT_TRUE(ImageStorage::Instance().AppendChunk("chunk-seq-1", 8, c3, 4));

    // 读取完整 blob 验证
    std::vector<uint8_t> out;
    ASSERT_TRUE(ImageStorage::Instance().ReadRange("chunk-seq-1", 0, 12, out));
    ASSERT_EQ(out.size(), 12u);
    uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44, 0xFF, 0xEE, 0xDD, 0xCC};
    EXPECT_EQ(std::memcmp(out.data(), expected, 12), 0);
}

TEST_F(ImageStorageTest, AppendChunk_OutOfOrderWrite)
{
    // 插入一条空 blob 的记录
    ImageRecord rec;
    rec.image_id = "chunk-ooo-1";
    rec.from_uid = 10;
    rec.to_uid = 20;
    rec.ext = "bin";
    rec.size = 8;
    rec.md5 = "def";
    rec.width = 1;
    rec.height = 1;
    rec.created_at = std::time(nullptr);
    rec.expires_at = rec.created_at + 3600;
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec));

    // 乱序写入：先写 offset=4，再写 offset=0
    uint8_t c2[] = {0x55, 0x66, 0x77, 0x88};
    uint8_t c1[] = {0x11, 0x22, 0x33, 0x44};
    ASSERT_TRUE(ImageStorage::Instance().AppendChunk("chunk-ooo-1", 4, c2, 4));
    ASSERT_TRUE(ImageStorage::Instance().AppendChunk("chunk-ooo-1", 0, c1, 4));

    // 读取完整 blob 验证：offset 0-3 应为 c1，offset 4-7 应为 c2
    std::vector<uint8_t> out;
    ASSERT_TRUE(ImageStorage::Instance().ReadRange("chunk-ooo-1", 0, 8, out));
    ASSERT_EQ(out.size(), 8u);
    uint8_t expected[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    EXPECT_EQ(std::memcmp(out.data(), expected, 8), 0);
}
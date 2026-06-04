#include "ImageStorage.h"
#include "SQLiteMgr.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <ctime>

class ImageStorageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _db_path = "test_image_storage_" + std::to_string(std::time(nullptr)) + "_" +
                   std::to_string(rand()) + ".db";
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
    rec.from_uid = 1; rec.to_uid = 2;
    rec.ext = "png"; rec.size = 100; rec.md5 = "x";
    rec.width = 10; rec.height = 10;
    rec.created_at = std::time(nullptr);
    rec.expires_at = rec.created_at + 3600;
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec));
    ASSERT_TRUE(ImageStorage::Instance().MarkRecalled("test-img-2"));
    auto got = ImageStorage::Instance().Get("test-img-2");
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(got->recalled);
}

TEST_F(ImageStorageTest, DeleteExpiredAndRecalled)
{
    ImageRecord rec;
    rec.image_id = "old-recalled";
    rec.from_uid = 1; rec.to_uid = 2;
    rec.ext = "png"; rec.size = 100; rec.md5 = "x";
    rec.width = 10; rec.height = 10;
    rec.created_at = 1000;
    rec.expires_at = 2000;  // 早就过期
    ASSERT_TRUE(ImageStorage::Instance().Insert(rec));
    ASSERT_TRUE(ImageStorage::Instance().MarkRecalled("old-recalled"));
    int deleted = ImageStorage::Instance().DeleteExpired(3000);
    EXPECT_GE(deleted, 1);
    EXPECT_FALSE(ImageStorage::Instance().Get("old-recalled").has_value());
}
#include <gtest/gtest.h>
#include "UserData.h"

TEST(UserDataTest, AddUserSuccess)
{
    auto &ud = UserData::Instance();
    EXPECT_TRUE(ud.AddUser(1001, "test_user_a", "hash_a"));
}

TEST(UserDataTest, AddDuplicateUser)
{
    auto &ud = UserData::Instance();
    ASSERT_TRUE(ud.AddUser(1002, "duplicate_check", "hash_1"));
    EXPECT_FALSE(ud.AddUser(1003, "duplicate_check", "hash_2"));
}

TEST(UserDataTest, GetPasswordHash)
{
    auto &ud = UserData::Instance();
    ASSERT_TRUE(ud.AddUser(2001, "pwd_user", "pwd_hash_val"));

    auto hash = ud.GetPasswordHash("pwd_user");
    ASSERT_TRUE(hash.has_value());
    EXPECT_EQ(*hash, "pwd_hash_val");
}

TEST(UserDataTest, GetPasswordHashMissing)
{
    auto &ud = UserData::Instance();
    EXPECT_FALSE(ud.GetPasswordHash("non_existent_user").has_value());
}

TEST(UserDataTest, GetUid)
{
    auto &ud = UserData::Instance();
    ASSERT_TRUE(ud.AddUser(3001, "uid_user", "secret"));

    auto uid = ud.GetUid("uid_user");
    ASSERT_TRUE(uid.has_value());
    EXPECT_EQ(*uid, 3001);
}

TEST(UserDataTest, GetUidMissing)
{
    auto &ud = UserData::Instance();
    EXPECT_FALSE(ud.GetUid("no_such_user").has_value());
}

TEST(UserDataTest, GetUsername)
{
    auto &ud = UserData::Instance();
    ASSERT_TRUE(ud.AddUser(4001, "name_user", "name_pwd"));

    auto name = ud.GetUsername(4001);
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "name_user");
}

TEST(UserDataTest, GetUsernameMissing)
{
    auto &ud = UserData::Instance();
    EXPECT_FALSE(ud.GetUsername(9999).has_value());
}

TEST(UserDataTest, MultipleUsers)
{
    auto &ud = UserData::Instance();
    EXPECT_TRUE(ud.AddUser(5001, "multi_a", "ha"));
    EXPECT_TRUE(ud.AddUser(5002, "multi_b", "hb"));
    EXPECT_TRUE(ud.AddUser(5003, "multi_c", "hc"));

    EXPECT_EQ(*ud.GetPasswordHash("multi_a"), "ha");
    EXPECT_EQ(*ud.GetPasswordHash("multi_b"), "hb");
    EXPECT_EQ(*ud.GetPasswordHash("multi_c"), "hc");
    EXPECT_EQ(*ud.GetUid("multi_b"), 5002);
    EXPECT_EQ(*ud.GetUsername(5003), "multi_c");
}

/**
 * @file test_AuthFlow.cpp
 * @brief Phase 4.5 T.1 — 认证流程集成测试
 * @details 测试 SQLiteMgr 认证业务方法 + TokenManager，
 *          覆盖注册→登录→Token 校验→验证码→重置密码全流程。
 */
#include "SQLiteMgr.h"
#include "AuthRepository.h"
#include "TokenManager.h"
#include "const.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

class AuthFlowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _db_path = "test_auth_flow_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand()) + ".db";
        ASSERT_TRUE(SQLiteMgr::Instance().Init(_db_path, 2));
    }

    void TearDown() override
    {
        SQLiteMgr::Instance().Shutdown();
        std::remove(_db_path.c_str());
    }

    std::string _db_path;
};

// ============================================================
// 注册测试
// ============================================================

TEST_F(AuthFlowTest, Register_Success)
{
    auto result = SQLiteMgr::Instance().Auth().RegisterUser("alice", "client_hash_abc123", "alice@test.com");
    EXPECT_EQ(result.error, ERR_SUCCESS);
    EXPECT_GT(result.uid, 0);
    EXPECT_FALSE(result.token.empty());
    EXPECT_EQ(result.username, "alice");
}

TEST_F(AuthFlowTest, Register_DuplicateUsername)
{
    auto first = SQLiteMgr::Instance().Auth().RegisterUser("bob", "hash1", "bob@test.com");
    ASSERT_EQ(first.error, ERR_SUCCESS);

    auto second = SQLiteMgr::Instance().Auth().RegisterUser("bob", "hash2", "bob2@test.com");
    EXPECT_EQ(second.error, ERR_USER_EXIST);
}

// ============================================================
// 登录测试
// ============================================================

TEST_F(AuthFlowTest, Login_Success)
{
    auto reg = SQLiteMgr::Instance().Auth().RegisterUser("charlie", "my_password_hash", "charlie@test.com");
    ASSERT_EQ(reg.error, ERR_SUCCESS);

    auto login = SQLiteMgr::Instance().Auth().LoginUser("charlie", "my_password_hash");
    EXPECT_EQ(login.error, ERR_SUCCESS);
    EXPECT_EQ(login.uid, reg.uid);
    EXPECT_FALSE(login.token.empty());
    EXPECT_EQ(login.username, "charlie");
}

TEST_F(AuthFlowTest, Login_WrongPassword)
{
    auto reg = SQLiteMgr::Instance().Auth().RegisterUser("dave", "correct_hash", "dave@test.com");
    ASSERT_EQ(reg.error, ERR_SUCCESS);

    auto login = SQLiteMgr::Instance().Auth().LoginUser("dave", "wrong_hash");
    EXPECT_EQ(login.error, ERR_PASSWD_ERR);
}

TEST_F(AuthFlowTest, Login_NonexistentUser)
{
    auto login = SQLiteMgr::Instance().Auth().LoginUser("ghost", "any_hash");
    EXPECT_EQ(login.error, ERR_USER_NOT_EXIST);
}

// ============================================================
// Token 测试
// ============================================================

TEST_F(AuthFlowTest, Token_SetAndCheck)
{
    // 使用独立 uid 避免与其他测试冲突（TokenManager 为进程级单例）
    constexpr int test_uid = 90001;
    const std::string test_token = "test_token_abc123";

    TokenManager::Instance().SetToken(test_uid, test_token);

    EXPECT_TRUE(TokenManager::Instance().CheckToken(test_uid, test_token));
    EXPECT_FALSE(TokenManager::Instance().CheckToken(test_uid, "wrong_token"));
    EXPECT_FALSE(TokenManager::Instance().CheckToken(99999, test_token));
}

// ============================================================
// 验证码 + 重置密码全流程测试
// ============================================================

TEST_F(AuthFlowTest, VerifyCode_FullFlow)
{
    // 1. 注册用户
    auto reg = SQLiteMgr::Instance().Auth().RegisterUser("eve", "old_password_hash", "eve@test.com");
    ASSERT_EQ(reg.error, ERR_SUCCESS);

    // 2. 发送验证码
    int code = 0;
    ASSERT_TRUE(SQLiteMgr::Instance().Auth().SendVerifyCode("eve@test.com", code));
    EXPECT_GE(code, 100000);
    EXPECT_LE(code, 999999);

    // 3. 用正确验证码校验
    std::string code_str = std::to_string(code);
    EXPECT_EQ(SQLiteMgr::Instance().Auth().CheckVerifyCode("eve@test.com", code_str), ERR_SUCCESS);

    // 4. 用错误验证码校验
    EXPECT_EQ(SQLiteMgr::Instance().Auth().CheckVerifyCode("eve@test.com", "000000"), ERR_VERIFY_WRONG);

    // 5. 重置密码（使用正确验证码）
    int reset_result = SQLiteMgr::Instance().Auth().ResetPassword("eve", "eve@test.com", code_str, "new_password_hash");
    EXPECT_EQ(reset_result, ERR_SUCCESS);

    // 6. 用新密码登录成功
    auto login_new = SQLiteMgr::Instance().Auth().LoginUser("eve", "new_password_hash");
    EXPECT_EQ(login_new.error, ERR_SUCCESS);

    // 7. 用旧密码登录失败
    auto login_old = SQLiteMgr::Instance().Auth().LoginUser("eve", "old_password_hash");
    EXPECT_EQ(login_old.error, ERR_PASSWD_ERR);
}

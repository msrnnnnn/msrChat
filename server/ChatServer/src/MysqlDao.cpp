#include "MysqlDao.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include <vector>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

MysqlDao::MysqlDao()
{
    pool_ = std::make_unique<MySqlPool>(
        ConfigMgr::GetInstance()["Mysql"]["Host"] + ":" + ConfigMgr::GetInstance()["Mysql"]["Port"], 
        ConfigMgr::GetInstance()["Mysql"]["User"], 
        ConfigMgr::GetInstance()["Mysql"]["Passwd"], 
        ConfigMgr::GetInstance()["Mysql"]["Name"], 
        5
    );
}

MysqlDao::~MysqlDao()
{
    pool_->Close();
}

int MysqlDao::RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return 0;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> stmt(con->prepareStatement("CALL reg_user(?,?,?,?,@result)"));
        stmt->setString(1, name);
        stmt->setString(2, email);
        stmt->setString(3, pwd);
        stmt->setString(4, icon);

        stmt->execute();

        std::unique_ptr<sql::Statement> stmtResult(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));

        if (res->next())
        {
            int result = res->getInt("result");
            spdlog::info("RegUser Result: {}", result);
            return result;
        }

        return -1;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return -1;
    }
}

bool MysqlDao::CheckEmail(const std::string &name, const std::string &email)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT email FROM user WHERE name = ?"));
        pstmt->setString(1, name);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        while (res->next())
        {
            if (email != res->getString("email"))
            {
                return false;
            }
            return true;
        }
        return false;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return false;
    }
}

bool MysqlDao::UpdatePwd(const std::string &name, const std::string &newpwd)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE user SET password = ? WHERE name = ?"));
        pstmt->setString(1, newpwd);
        pstmt->setString(2, name);
        int updateCount = pstmt->executeUpdate();
        return updateCount > 0;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return false;
    }
}

bool MysqlDao::CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM user WHERE name = ?"));
        pstmt->setString(1, name);

        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        if (res->next())
        {
            std::string origin_pwd = res->getString("password");
            
            if (pwd != origin_pwd)
            {
                return false;
            }

            userInfo.name = name;
            userInfo.email = res->getString("email");
            userInfo.uid = res->getInt("uid");
            userInfo.pwd = ""; 
            return true;
        }
        
        return false;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return false;
    }
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(std::string name)
{
    auto con = pool_->getConnection();
    if(con == nullptr) return nullptr;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM user WHERE name = ?"));
        pstmt->setString(1, name);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if(res->next()) {
             auto user_ptr = std::make_shared<UserInfo>();
             user_ptr->uid = res->getInt("uid");
             user_ptr->name = res->getString("name");
             user_ptr->pwd = res->getString("password");
             user_ptr->email = res->getString("email");
             user_ptr->nick = res->getString("nick");
             try {
                 user_ptr->desc = res->getString("desc");
             } catch (...) {
                 spdlog::warn("Column 'desc' missing or invalid, using empty string");
                 user_ptr->desc = "";
             }
             user_ptr->sex = res->getInt("sex");
             user_ptr->icon = res->getString("icon");
             return user_ptr;
        }
        return nullptr;
    }
    catch (sql::SQLException &e) {
        spdlog::error("SQLException: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
    auto con = pool_->getConnection();
    if(con == nullptr) return nullptr;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if(res->next()) {
             auto user_ptr = std::make_shared<UserInfo>();
             user_ptr->uid = res->getInt("uid");
             user_ptr->name = res->getString("name");
             user_ptr->pwd = res->getString("password");
             user_ptr->email = res->getString("email");
             user_ptr->nick = res->getString("nick");
             user_ptr->desc = res->getString("desc");
             user_ptr->sex = res->getInt("sex");
             user_ptr->icon = res->getString("icon");
             return user_ptr;
        }
        return nullptr;
    }
    catch (sql::SQLException &e) {
        spdlog::error("SQLException: {}", e.what());
        return nullptr;
    }
}

bool MysqlDao::AddFriendApply(int from_uid, int to_uid, const std::string& msg)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO friend_apply (from_uid, to_uid, msg) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE msg = VALUES(msg)"));
        pstmt->setInt(1, from_uid);
        pstmt->setInt(2, to_uid);
        pstmt->setString(3, msg);
        int updateCount = pstmt->executeUpdate();
        return true;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return false;
    }
}

bool MysqlDao::AuthFriendApply(int from_uid, int to_uid)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE friend_apply SET status = 1 WHERE from_uid = ? AND to_uid = ?"));
        pstmt->setInt(1, from_uid);
        pstmt->setInt(2, to_uid);
        int updateCount = pstmt->executeUpdate();
        return updateCount > 0;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return false;
    }
}

bool MysqlDao::AddFriend(int self_id, int friend_id, const std::string& back)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT IGNORE INTO friend (self_id, friend_id, back) VALUES (?, ?, ?)"));
        pstmt->setInt(1, self_id);
        pstmt->setInt(2, friend_id);
        pstmt->setString(3, back);
        int updateCount = pstmt->executeUpdate();
        return true;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return false;
    }
}

std::vector<std::shared_ptr<ApplyInfo>> MysqlDao::GetApplyList(int uid, int offset, int limit)
{
    std::vector<std::shared_ptr<ApplyInfo>> applyList;
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return applyList;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT fa.from_uid, fa.msg, fa.status, u.name, u.nick, u.sex, u.icon FROM friend_apply fa JOIN user u ON fa.from_uid = u.uid WHERE fa.to_uid = ? ORDER BY fa.created_at DESC LIMIT ? OFFSET ?"));
        pstmt->setInt(1, uid);
        pstmt->setInt(2, limit);
        pstmt->setInt(3, offset);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res->next())
        {
            auto applyInfo = std::make_shared<ApplyInfo>();
            applyInfo->applyuid = res->getInt("from_uid");
            applyInfo->desc = res->getString("msg");
            applyInfo->status = res->getInt("status");
            applyInfo->name = res->getString("name");
            applyInfo->nick = res->getString("nick");
            applyInfo->sex = res->getInt("sex");
            applyInfo->icon = res->getString("icon");
            applyList.push_back(applyInfo);
        }
        return applyList;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return applyList;
    }
}

std::vector<std::shared_ptr<UserInfo>> MysqlDao::GetFriendList(int uid)
{
    std::vector<std::shared_ptr<UserInfo>> friendList;
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return friendList;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT u.uid, u.name, u.nick, u.sex, u.icon, u.email, f.back FROM friend f JOIN user u ON f.friend_id = u.uid WHERE f.self_id = ?"));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res->next())
        {
            auto userInfo = std::make_shared<UserInfo>();
            userInfo->uid = res->getInt("uid");
            userInfo->name = res->getString("name");
            userInfo->nick = res->getString("nick");
            userInfo->sex = res->getInt("sex");
            userInfo->icon = res->getString("icon");
            userInfo->email = res->getString("email");
            friendList.push_back(userInfo);
        }
        return friendList;
    }
    catch (sql::SQLException &e)
    {
        spdlog::error("SQLException: {}, MySQL error code: {}, SQLState: {}", e.what(), e.getErrorCode(), e.getSQLState().c_str());
        return friendList;
    }
}

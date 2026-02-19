#include "MysqlDao.h"

MysqlDao::MysqlDao()
{
    auto &cfg = ConfigMgr::GetInstance();
    const auto &host = cfg["Mysql"]["Host"];
    const auto &port = cfg["Mysql"]["Port"];
    const auto &pwd = cfg["Mysql"]["Passwd"];
    const auto &schema = cfg["Mysql"]["Name"]; // 注意 config.ini 里叫 Name 还是 Schema，要对应
    const auto &user = cfg["Mysql"]["User"];

    // 初始化连接池，这里拼出了 tcp://192.168.x.x:3306
    pool_.reset(new MySqlPool("tcp://" + host + ":" + port, user, pwd, schema, 5));
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
        // Check if user or email exists
        std::unique_ptr<sql::PreparedStatement> stmt(con->prepareStatement("SELECT uid FROM user WHERE name = ? OR email = ?"));
        stmt->setString(1, name);
        stmt->setString(2, email);
        
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (res->next())
        {
            pool_->returnConnection(std::move(con));
            return 0; // User or email already exists
        }

        // Insert new user
        stmt.reset(con->prepareStatement("INSERT INTO user (name, email, password) VALUES (?, ?, ?)"));
        stmt->setString(1, name);
        stmt->setString(2, email);
        stmt->setString(3, pwd);
        
        int updateCount = stmt->executeUpdate();
        if (updateCount > 0)
        {
            // Get the generated uid
            std::unique_ptr<sql::Statement> stmtResult(con->createStatement());
            std::unique_ptr<sql::ResultSet> resUid(stmtResult->executeQuery("SELECT LAST_INSERT_ID()"));
            if (resUid->next())
            {
                int uid = resUid->getInt(1);
                std::cout << "RegUser Success, uid: " << uid << std::endl;
                pool_->returnConnection(std::move(con));
                return uid;
            }
        }

        pool_->returnConnection(std::move(con));
        return -1;
    }
    catch (sql::SQLException &e)
    {
        pool_->returnConnection(std::move(con));
        std::cerr << "SQLException: " << e.what() << std::endl;
        if (e.getErrorCode() == 1062) // Duplicate entry
        {
            return 0;
        }
        return -1;
    }
}

int MysqlDao::ResetPwd(const std::string &name, const std::string &email, const std::string &pwd)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return -1;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement("SELECT uid FROM user WHERE name = ? AND email = ?"));
        stmt->setString(1, name);
        stmt->setString(2, email);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (!res->next())
        {
            pool_->returnConnection(std::move(con));
            return 0;
        }
        int uid = res->getInt(1);

        stmt.reset(con->prepareStatement("UPDATE user SET password = ? WHERE uid = ?"));
        stmt->setString(1, pwd);
        stmt->setInt(2, uid);
        int updateCount = stmt->executeUpdate();
        pool_->returnConnection(std::move(con));
        if (updateCount > 0)
        {
            return uid;
        }
        return -1;
    }
    catch (sql::SQLException &e)
    {
        pool_->returnConnection(std::move(con));
        std::cerr << "SQLException: " << e.what() << std::endl;
        return -1;
    }
}

int MysqlDao::LoginUser(const std::string &name, const std::string &pwd)
{
    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        return -1;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement("SELECT uid, password FROM user WHERE name = ?"));
        stmt->setString(1, name);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (!res->next())
        {
            pool_->returnConnection(std::move(con));
            return 0;
        }
        int uid = res->getInt("uid");
        std::string db_pwd = res->getString("password");
        pool_->returnConnection(std::move(con));
        if (db_pwd != pwd)
        {
            return -1;
        }
        return uid;
    }
    catch (sql::SQLException &e)
    {
        pool_->returnConnection(std::move(con));
        std::cerr << "SQLException: " << e.what() << std::endl;
        return -1;
    }
}

bool MysqlDao::CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo)
{
    userInfo.uid = 0;
    userInfo.name = name;
    userInfo.email.clear();
    userInfo.pwd.clear();

    auto con = pool_->getConnection();
    if (con == nullptr)
    {
        userInfo.uid = -1;
        return false;
    }

    try
    {
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement("SELECT uid, email, password FROM user WHERE name = ?"));
        stmt->setString(1, name);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (!res->next())
        {
            pool_->returnConnection(std::move(con));
            userInfo.uid = 0;
            return false;
        }

        int uid = res->getInt("uid");
        std::string db_pwd = res->getString("password");
        std::string email = res->getString("email");
        pool_->returnConnection(std::move(con));

        if (db_pwd != pwd)
        {
            userInfo.uid = -1;
            return false;
        }

        userInfo.uid = uid;
        userInfo.email = email;
        userInfo.pwd = db_pwd;
        return true;
    }
    catch (sql::SQLException &e)
    {
        pool_->returnConnection(std::move(con));
        std::cerr << "SQLException: " << e.what() << std::endl;
        userInfo.uid = -1;
        return false;
    }
}

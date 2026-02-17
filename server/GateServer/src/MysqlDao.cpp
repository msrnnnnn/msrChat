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
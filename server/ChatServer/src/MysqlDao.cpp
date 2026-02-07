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
        // 【修改点】这里变成 4 个问号，对应 reg_user 存储过程的参数
        std::unique_ptr<sql::PreparedStatement> stmt(con->prepareStatement("CALL reg_user(?,?,?,?,@result)"));

        // 设置输入参数
        stmt->setString(1, name);
        stmt->setString(2, email);
        stmt->setString(3, pwd);
        stmt->setString(4, icon); // 【修改点】设置头像

        // 执行存储过程
        stmt->execute();

        // 获取输出参数 @result
        std::unique_ptr<sql::Statement> stmtResult(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));

        if (res->next())
        {
            int result = res->getInt("result");
            std::cout << "RegUser Result: " << result << std::endl;
            return result;
        }

        return -1;
    }
    catch (sql::SQLException &e)
    {
        std::cerr << "SQLException: " << e.what() << std::endl;
        return -1;
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
             user_ptr->pwd = res->getString("pwd");
             user_ptr->email = res->getString("email");
             return user_ptr;
        }
        return nullptr;
    }
    catch (sql::SQLException &e) {
        std::cerr << "SQLException: " << e.what() << std::endl;
        return nullptr;
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
        // 准备SQL语句
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM user WHERE name = ?"));
        pstmt->setString(1, name);

        // 执行查询
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        std::string origin_pwd = "";
        
        // 遍历结果集
        if (res->next())
        {
            origin_pwd = res->getString("pwd");
            std::cout << "Password: " << origin_pwd << std::endl;
            
            if (pwd != origin_pwd)
            {
                return false;
            }

            userInfo.name = name;
            userInfo.email = res->getString("email");
            userInfo.uid = res->getInt("uid");
            userInfo.pwd = origin_pwd;
            return true;
        }
        
        return false;
    }
    catch (sql::SQLException &e)
    {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
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
            std::cout << "Check Email: " << res->getString("email") << std::endl;
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
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
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
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
        pstmt->setString(1, newpwd);
        pstmt->setString(2, name);
        int updateCount = pstmt->executeUpdate();
        std::cout << "Updated rows: " << updateCount << std::endl;
        return updateCount > 0;
    }
    catch (sql::SQLException &e)
    {
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}

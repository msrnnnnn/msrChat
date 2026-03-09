#include "MysqlMgr.h"

MysqlMgr::MysqlMgr()
{
}
MysqlMgr::~MysqlMgr()
{
}

int MysqlMgr::RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon)
{
    // 调用 DAO 层
    return _dao.RegUser(name, email, pwd, icon);
}

int MysqlMgr::ResetPwd(const std::string &name, const std::string &email, const std::string &pwd)
{
    return _dao.ResetPwd(name, email, pwd);
}

int MysqlMgr::LoginUser(const std::string &name, const std::string &pwd)
{
    return _dao.LoginUser(name, pwd);
}

bool MysqlMgr::CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo)
{
    return _dao.CheckPwd(name, pwd, userInfo);
}

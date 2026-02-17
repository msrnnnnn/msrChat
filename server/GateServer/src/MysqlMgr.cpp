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
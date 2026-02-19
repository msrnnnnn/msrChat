/**
 * @file MysqlMgr.h
 * @brief MySQL 管理器定义
 * @details 封装了 MySQLDao，提供统一的数据库操作接口。
 */
#pragma once

#include "MysqlDao.h"
#include "Singleton.h"

/**
 * @class   MysqlMgr
 * @brief   MySQL 管理器 (Singleton)
 */
class MysqlMgr : public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr();

    /**
     * @brief   注册用户
     * @param   name  用户名
     * @param   email 邮箱地址
     * @param   pwd   密码
     * @param   icon  头像 URL
     * @return  int   注册结果
     */
    int RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon);
    int ResetPwd(const std::string &name, const std::string &email, const std::string &pwd);
    int LoginUser(const std::string &name, const std::string &pwd);
    bool CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo);

private:
    MysqlMgr();
    MysqlDao _dao; ///< MySQL DAO 对象
};

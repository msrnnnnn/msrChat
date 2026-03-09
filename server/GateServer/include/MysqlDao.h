/**
 * @file MysqlDao.h
 * @brief MySQL 数据访问对象 (DAO) 定义
 * @details 负责执行具体的数据库操作（CRUD）。
 */
#pragma once

#include "ConfigMgr.h"
#include "MysqlPool.h"
#include <memory>
#include <string>

struct UserInfo
{
    int uid = 0;
    std::string name;
    std::string email;
    std::string pwd;
};

/**
 * @class   MysqlDao
 * @brief   MySQL 数据访问对象
 * @details 封装了用户相关的数据库操作接口。
 */
class MysqlDao
{
public:
    MysqlDao();
    ~MysqlDao();

    /**
     * @brief   注册用户
     * @param   name  用户名
     * @param   email 邮箱
     * @param   pwd   密码
     * @param   icon  头像
     * @return  注册结果代码
     */
    int RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon);

    /**
     * @brief   重置密码
     * @param   name  用户名
     * @param   email 邮箱
     * @param   pwd   新密码
     * @return  操作结果代码
     */
    int ResetPwd(const std::string &name, const std::string &email, const std::string &pwd);

    /**
     * @brief   用户登录验证
     * @param   name  用户名
     * @param   pwd   密码
     * @return  操作结果代码
     */
    int LoginUser(const std::string &name, const std::string &pwd);

    /**
     * @brief   校验密码并获取用户信息
     * @param   name      用户名
     * @param   pwd       密码
     * @param   userInfo  输出参数：用户信息
     * @return  true 验证通过
     */
    bool CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo);

private:
    std::unique_ptr<MySqlPool> pool_;
};

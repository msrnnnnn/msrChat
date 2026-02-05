/**
 * @file MysqlDao.h
 * @brief MySQL 数据访问对象 (DAO) 定义
 * @author msr
 */

#pragma once

#include "ConfigMgr.h"
#include "MysqlPool.h"
#include <memory>
#include <string>

/**
 * @class   MysqlDao
 * @brief   MySQL 数据访问对象
 *
 * @details 负责处理与 MySQL 数据库的具体交互，如注册用户、查询用户信息等。
 *          内部持有 MySQL 连接池。
 */
class MysqlDao
{
public:
    /**
     * @brief   构造函数
     * @details 初始化 MySQL 连接池
     */
    MysqlDao();

    /**
     * @brief   析构函数
     * @details 关闭连接池
     */
    ~MysqlDao();

    /**
     * @brief   注册用户
     * @param   name  用户名
     * @param   email 邮箱地址
     * @param   pwd   密码
     * @param   icon  头像 URL 或标识
     * @return  int   注册结果 (通常由存储过程返回)
     */
    int RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon);

private:
    std::unique_ptr<MySqlPool> pool_; ///< MySQL 连接池
};
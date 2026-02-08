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

struct UserInfo
{
    std::string name;
    std::string pwd;
    int uid;
    std::string email;
    std::string nick;
    std::string desc;
    int sex;
    std::string icon;
};

struct ApplyInfo {
    int applyuid;
    std::string name;
    std::string desc;
    std::string icon;
    std::string nick;
    int sex;
    int status;
};

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

    /**
     * @brief   检查邮箱与用户名是否匹配
     * @param   name  用户名
     * @param   email 邮箱地址
     * @return  bool  匹配返回 true，否则 false
     */
    bool CheckEmail(const std::string &name, const std::string &email);

    /**
     * @brief   更新用户密码
     * @param   name  用户名
     * @param   pwd   新密码
     * @return  bool  更新成功返回 true，否则 false
     */
    bool UpdatePwd(const std::string &name, const std::string &pwd);

    /**
     * @brief   验证用户密码
     * @param   name     用户名
     * @param   pwd      密码
     * @param   userInfo [out] 用户信息
     * @return  bool     验证成功返回 true，否则 false
     */
    bool CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo);

    std::shared_ptr<UserInfo> GetUser(int uid);
    std::shared_ptr<UserInfo> GetUser(std::string name);
    bool AddFriendApply(int from_uid, int to_uid, const std::string& msg);
    bool AuthFriendApply(int from_uid, int to_uid);
    bool AddFriend(int self_id, int friend_id, const std::string& back);
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList(int uid, int offset, int limit);
    std::vector<std::shared_ptr<UserInfo>> GetFriendList(int uid);

private:
    std::unique_ptr<MySqlPool> pool_; ///< MySQL 连接池
};

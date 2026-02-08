/**
 * @file MysqlMgr.h
 * @brief MySQL 管理器定义
 * @author msr
 */

#pragma once

#include "MysqlDao.h"
#include "Singleton.h"

/**
 * @class   MysqlMgr
 * @brief   MySQL 管理器 (Singleton)
 *
 * @details 封装了 MySQLDao，提供统一的数据库操作接口。
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

    /**
     * @brief   验证用户密码
     * @param   name     用户名
     * @param   pwd      密码
     * @param   userInfo [out] 用户信息
     * @return  bool     验证成功返回 true，否则 false
     */
    bool CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo);

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

    std::shared_ptr<UserInfo> GetUser(int uid);
    std::shared_ptr<UserInfo> GetUser(std::string name);
    bool AddFriendApply(int from_uid, int to_uid, const std::string& msg);
    bool AuthFriendApply(int from_uid, int to_uid);
    bool AddFriend(int self_id, int friend_id, const std::string& back);
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList(int uid, int offset, int limit);
    std::vector<std::shared_ptr<UserInfo>> GetFriendList(int uid);

private:
    MysqlMgr();
    MysqlDao _dao; ///< MySQL DAO 对象
};

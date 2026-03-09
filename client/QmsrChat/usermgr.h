/**
 * @file usermgr.h
 * @brief 用户数据管理单例类
 * @details 存储当前登录用户的 UID、用户名和 Token。
 */
#ifndef USERMGR_H
#define USERMGR_H

#include "singleton.h"
#include <QObject>
#include <QString>

/**
 * @class UserMgr
 * @brief 用户数据管理类
 * @details 继承自 Singleton<UserMgr>，提供线程安全的单例访问。
 */
class UserMgr : public Singleton<UserMgr>
{
    friend class Singleton<UserMgr>;

public:
    /**
     * @brief 设置用户 ID
     * @param uid 用户 ID
     */
    void SetUid(int uid);

    /**
     * @brief 获取用户 ID
     * @return int 用户 ID
     */
    int GetUid() const;

    /**
     * @brief 设置用户名
     * @param name 用户名
     */
    void SetName(const QString &name);

    /**
     * @brief 获取用户名
     * @return QString 用户名
     */
    QString GetName() const;

    /**
     * @brief 设置登录令牌
     * @param token 登录令牌
     */
    void SetToken(const QString &token);

    /**
     * @brief 获取登录令牌
     * @return QString 登录令牌
     */
    QString GetToken() const;

private:
    /**
     * @brief 私有构造函数
     */
    UserMgr() = default;

    int _uid = 0;        ///< 用户 ID
    QString _name = "";  ///< 用户名
    QString _token = ""; ///< 登录令牌
};

#endif // USERMGR_H

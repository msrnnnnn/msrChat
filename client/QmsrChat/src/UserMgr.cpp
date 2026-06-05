/**
 * @file UserMgr.cpp
 * @brief 用户数据管理实现
 */
#include "UserMgr.h"

/**
 * @brief 设置用户 ID
 * @param uid 用户 ID
 */
void UserMgr::SetUid(int uid)
{
    _uid = uid;
}

/**
 * @brief 获取用户 ID
 * @return int 用户 ID
 */
int UserMgr::GetUid() const
{
    return _uid;
}

/**
 * @brief 设置登录令牌
 * @param token 登录令牌
 */
void UserMgr::SetToken(const QString &token)
{
    _token = token;
}

/**
 * @brief 获取登录令牌
 * @return QString 登录令牌
 */
QString UserMgr::GetToken() const
{
    return _token;
}

/**
 * @file usermgr.cpp
 * @brief 用户数据管理实现
 */
#include "usermgr.h"

void UserMgr::SetUid(int uid)
{
    _uid = uid;
}

int UserMgr::GetUid() const
{
    return _uid;
}

void UserMgr::SetName(const QString &name)
{
    _name = name;
}

QString UserMgr::GetName() const
{
    return _name;
}

void UserMgr::SetToken(const QString &token)
{
    _token = token;
}

QString UserMgr::GetToken() const
{
    return _token;
}

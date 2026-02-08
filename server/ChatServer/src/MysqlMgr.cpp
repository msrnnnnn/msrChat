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

bool MysqlMgr::CheckPwd(const std::string &name, const std::string &pwd, UserInfo &userInfo)
{
    return _dao.CheckPwd(name, pwd, userInfo);
}

bool MysqlMgr::CheckEmail(const std::string &name, const std::string &email)
{
    return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string &name, const std::string &pwd)
{
    return _dao.UpdatePwd(name, pwd);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
    return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(std::string name)
{
    return _dao.GetUser(name);
}

bool MysqlMgr::AddFriendApply(int from_uid, int to_uid, const std::string& msg)
{
    return _dao.AddFriendApply(from_uid, to_uid, msg);
}

bool MysqlMgr::AuthFriendApply(int from_uid, int to_uid)
{
    return _dao.AuthFriendApply(from_uid, to_uid);
}

bool MysqlMgr::AddFriend(int self_id, int friend_id, const std::string& back)
{
    return _dao.AddFriend(self_id, friend_id, back);
}

std::vector<std::shared_ptr<ApplyInfo>> MysqlMgr::GetApplyList(int uid, int offset, int limit)
{
    return _dao.GetApplyList(uid, offset, limit);
}

std::vector<std::shared_ptr<UserInfo>> MysqlMgr::GetFriendList(int uid)
{
    return _dao.GetFriendList(uid);
}

#include "SessionManager.h"
#include "CSession.h"

void SessionManager::AddSession(int uid, std::shared_ptr<CSession> session)
{
    if (!session)
    {
        return;
    }

    std::string uuid = session->GetUuid();
    _uid_sessions.Insert(uid, session);
    _uuid_sessions.Insert(uuid, session);
}

void SessionManager::RemoveSession(int uid)
{
    auto session = _uid_sessions.Find(uid);
    if (session)
    {
        std::string uuid = (*session)->GetUuid();
        _uuid_sessions.Erase(uuid);
    }
    _uid_sessions.Erase(uid);
}

void SessionManager::RemoveSessionByUuid(const std::string &uuid)
{
    auto session = _uuid_sessions.Find(uuid);
    if (session)
    {
        int uid = (*session)->GetUserUid();
        _uid_sessions.Erase(uid);
    }
    _uuid_sessions.Erase(uuid);
}

std::shared_ptr<CSession> SessionManager::GetSession(int uid) const
{
    auto *session = _uid_sessions.Find(uid);
    return session ? *session : nullptr;
}

std::shared_ptr<CSession> SessionManager::GetSessionByUuid(const std::string &uuid) const
{
    auto *session = _uuid_sessions.Find(uuid);
    return session ? *session : nullptr;
}

std::size_t SessionManager::SessionCount() const
{
    std::size_t total = 0;
    for (std::size_t i = 0; i < _uid_sessions.ShardCount(); ++i)
    {
        auto lock = _uid_sessions.GetLock(i);
        total += _uid_sessions.GetShard(i).size();
    }
    return total;
}

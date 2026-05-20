#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include "ShardedMap.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class CSession;

class SessionManager
{
public:
    static SessionManager &Instance()
    {
        static SessionManager instance;
        return instance;
    }

    void AddSession(int uid, std::shared_ptr<CSession> session);
    void RemoveSession(int uid);
    void RemoveSessionByUuid(const std::string &uuid);
    std::shared_ptr<CSession> GetSession(int uid) const;
    std::shared_ptr<CSession> GetSessionByUuid(const std::string &uuid) const;
    void ClearAll();

    template <typename Func>
    void ForEachSession(Func &&func)
    {
        _uid_sessions.ForEach([&func](int uid, const std::shared_ptr<CSession> &session) {
            func(uid, session);
        });
    }

    std::size_t SessionCount() const;

private:
    SessionManager() = default;
    ~SessionManager() = default;

    SessionManager(const SessionManager &) = delete;
    SessionManager &operator=(const SessionManager &) = delete;

    SessionManager(SessionManager &&) = delete;
    SessionManager &operator=(SessionManager &&) = delete;

    ShardedMap<int, std::shared_ptr<CSession>> _uid_sessions{32};
    ShardedMap<std::string, std::shared_ptr<CSession>> _uuid_sessions{32};
};

#endif

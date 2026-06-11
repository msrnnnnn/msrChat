#pragma once
/**
 * @file SessionManager.h
 * @brief 会话管理器 —— 维护在线用户的会话映射
 * @details 使用 ShardedMap 存储 uid->session 和 uuid->session 的双向映射，
 *          支持按 uid 或 uuid 查找、添加、移除会话。分片锁设计保证高并发读写性能。
 */
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

/**
 * @brief 会话管理器（单例）
 * @details 持有两个分片映射表：
 *          - `_uid_sessions`：按用户 ID 查找会话
 *          - `_uuid_sessions`：按连接 UUID 查找会话
 *          用户上线时 AddSession，离线时 RemoveSession。
 */
class SessionManager
{
public:
    static SessionManager &Instance()
    {
        static SessionManager instance;
        return instance;
    }

    /**
     * @brief 添加会话（同时写入 uid 和 uuid 两个映射表）
     */
    void AddSession(int uid, std::shared_ptr<CSession> session);

    /**
     * @brief 按 uid 移除会话（同时清理对应的 uuid 映射）
     */
    void RemoveSession(int uid);

    /**
     * @brief 按连接 uuid 移除会话（同时清理对应的 uid 映射）
     */
    void RemoveSessionByUuid(const std::string &uuid);

    std::shared_ptr<CSession> GetSession(int uid) const;

    /**
     * @brief 清空全部会话映射
     */
    void ClearAll();

    /**
     * @brief 获取当前在线连接数
     */
    size_t GetConnectionCount() const;

    /**
     * @brief 遍历全部在线会话（逐个分片加锁，回调中不应阻塞或操作同一 ShardedMap）
     */
    template <typename Func>
    void ForEachSession(Func &&func)
    {
        _uid_sessions.ForEach([&func](int uid, const std::shared_ptr<CSession> &session) {
            func(uid, session);
        });
    }

private:
    SessionManager() = default;
    ~SessionManager() = default;

    SessionManager(const SessionManager &) = delete;
    SessionManager &operator=(const SessionManager &) = delete;

    SessionManager(SessionManager &&) = delete;
    SessionManager &operator=(SessionManager &&) = delete;

    ShardedMap<int, std::shared_ptr<CSession>> _uid_sessions{32};
    ShardedMap<std::string, std::shared_ptr<CSession>> _uuid_sessions{32};
    std::mutex _add_mutex;  ///< 保护 AddSession 的 check-then-insert 原子性
};

#endif

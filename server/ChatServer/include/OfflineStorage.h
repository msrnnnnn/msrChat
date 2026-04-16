#ifndef OFFLINE_STORAGE_H
#define OFFLINE_STORAGE_H

#include "MessageRouter.h"
#include "SessionManager.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

class CSession;

class OfflineStorage
{
public:
    static OfflineStorage &Instance()
    {
        static OfflineStorage instance;
        return instance;
    }

    void StoreMessage(int target_uid, const std::string &msg_data);
    void SendOfflineMessages(int uid, std::shared_ptr<CSession> session);
    std::vector<std::string> GetOfflineMessages(int uid) const;
    void ClearOfflineMessages(int uid);
    std::size_t GetOfflineMessageCount(int uid) const;

private:
    OfflineStorage() = default;
    ~OfflineStorage() = default;

    OfflineStorage(const OfflineStorage &) = delete;
    OfflineStorage &operator=(const OfflineStorage &) = delete;

    OfflineStorage(OfflineStorage &&) = delete;
    OfflineStorage &operator=(OfflineStorage &&) = delete;

    mutable std::shared_mutex _mutex;
    std::unordered_map<int, std::vector<std::string>> _offline_messages;
};

#endif

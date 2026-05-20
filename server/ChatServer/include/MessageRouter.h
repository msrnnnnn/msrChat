#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "SessionManager.h"
#include <atomic>
#include <memory>
#include <string>

class CSession;

class MessageRouter
{
public:
    static MessageRouter &Instance()
    {
        static MessageRouter instance;
        return instance;
    }

    bool ForwardMessage(int target_uid, const std::string &msg_data);
    bool BroadcastMessage(const std::string &msg_data, int exclude_uid = 0);

    bool SendToSession(std::shared_ptr<CSession> session, const std::string &msg_data, short msg_id);

private:
    MessageRouter() = default;
    ~MessageRouter() = default;

    MessageRouter(const MessageRouter &) = delete;
    MessageRouter &operator=(const MessageRouter &) = delete;

    MessageRouter(MessageRouter &&) = delete;
    MessageRouter &operator=(MessageRouter &&) = delete;

    static std::atomic<int64_t> _next_server_msg_id;
};

#endif

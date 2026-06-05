#ifndef MESSAGEDISPATCHER_H
#define MESSAGEDISPATCHER_H
#include "const.h"
#include "CSession.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

using MessageHandler = std::function<bool(CSession &session, const std::string &body_data)>;

struct HandlerInfo
{
    MessageHandler handler;
    bool requires_auth;
};

class MessageDispatcher
{
public:
    static MessageDispatcher &Instance()
    {
        static MessageDispatcher instance;
        return instance;
    }

    void RegisterHandler(uint16_t msg_id, MessageHandler handler, bool requires_auth = false)
    {
        _handlers[msg_id] = {std::move(handler), requires_auth};
    }

    bool Dispatch(CSession &session, uint16_t msg_id, const std::string &body_data) const
    {
        auto it = _handlers.find(msg_id);
        if (it == _handlers.end())
        {
            return false;
        }

        const auto &info = it->second;
        if (info.requires_auth && session.GetUserUid() == 0)
        {
            return false;
        }

        return info.handler(session, body_data);
    }

private:
    MessageDispatcher()
    {
        RegisterDefaultHandlers();
    }

    MessageDispatcher(const MessageDispatcher &) = delete;
    MessageDispatcher &operator=(const MessageDispatcher &) = delete;

    void RegisterDefaultHandlers();

    std::unordered_map<uint16_t, HandlerInfo> _handlers;
};

#endif // MESSAGEDISPATCHER_H

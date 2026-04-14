/**
 * @file CServer.cpp
 * @brief 聊天服务 TCP 入口实现
 */
#include "CServer.h"
#include "CSession.h"
#include "SQLiteMgr.h"
#include "const.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

CServer::CServer(boost::asio::io_context &io_context, short port)
    : _io_context(io_context),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    _uid_tokens.Insert(1001, "dev_token");
    spdlog::info("[CServer] Server initialized on port {}", port);
}

void CServer::Start()
{
    DoAccept();
}

void CServer::DoAccept()
{
    auto new_session = std::make_shared<CSession>(_io_context, this);
    _acceptor.async_accept(
        new_session->GetSocket(),
        [this, new_session](const boost::system::error_code &ec)
        {
            if (!ec)
            {
                spdlog::info("[CServer] New connection accepted: {}", new_session->GetUuid());
                _uuid_sessions.Insert(new_session->GetUuid(), new_session);
                new_session->Start();
            }
            else
            {
                spdlog::error("[CServer] Accept error: {}", ec.message());
            }
            DoAccept();
        });
}

void CServer::AddUserSession(int uid, std::shared_ptr<CSession> session)
{
    _uid_sessions.Insert(uid, std::move(session));
    spdlog::info("[CServer] User {} session added.", uid);
}

void CServer::RemoveUserSession(int uid)
{
    if (_uid_sessions.Erase(uid))
    {
        spdlog::info("[CServer] User {} session removed.", uid);
    }
}

void CServer::ClearSession(const std::string &uuid)
{
    if (_uuid_sessions.Erase(uuid))
    {
        spdlog::info("[CServer] Session {} cleared.", uuid);
    }
}

bool CServer::ForwardMessage(int target_uid, const std::string &msg_data)
{
    auto target_session = _uid_sessions.Find(target_uid).value_or(nullptr);
    if (!target_session)
    {
        return false;
    }
    target_session->Send(msg_data, MSG_CHAT_TEXT);
    return true;
}

void CServer::StoreOfflineMessage(int target_uid, const std::string &msg_data)
{
    try
    {
        auto json_data = nlohmann::json::parse(msg_data);
        ChatMessage msg;
        msg.from_uid = json_data.value("from_uid", 0);
        msg.to_uid = target_uid;
        msg.content = json_data.value("content", "");
        msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        msg.status = 0;
        SQLiteMgr::Instance().SaveOfflineMessage(msg);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CServer] StoreOfflineMessage failed: {}", e.what());
    }
}

void CServer::SendOfflineMessages(int uid, std::shared_ptr<CSession> session)
{
    auto messages = SQLiteMgr::Instance().GetOfflineMessages(uid);
    for (const auto &msg : messages)
    {
        nlohmann::json forward;
        forward["from_uid"] = msg.from_uid;
        forward["to_uid"] = msg.to_uid;
        forward["content"] = msg.content;
        session->Send(forward.dump(), MSG_CHAT_TEXT);
    }
    SQLiteMgr::Instance().ClearOfflineMessages(uid);
}

void CServer::SetToken(int uid, const std::string &token)
{
    _uid_tokens.Insert(uid, token);
}

bool CServer::CheckToken(int uid, const std::string &token)
{
    auto stored = _uid_tokens.Find(uid);
    if (!stored.has_value())
    {
        return false;
    }
    return stored.value() == token;
}

void CServer::RemoveToken(int uid)
{
    _uid_tokens.Erase(uid);
}

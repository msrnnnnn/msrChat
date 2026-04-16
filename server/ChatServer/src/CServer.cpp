/**
 * @file CServer.cpp
 * @brief 聊天服务 TCP 入口实现
 */
#include "CServer.h"
#include "AsioIOServicePool.h"
#include "CSession.h"
#include "SQLiteMgr.h"
#include "const.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

CServer::CServer(boost::asio::io_context &io_context, short port)
    : _io_context(io_context),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      _thread_pool(std::thread::hardware_concurrency())
{
    spdlog::info("[CServer] Server initialized on port {}", port);
}

CServer::~CServer()
{
    _thread_pool.Shutdown();
}

void CServer::Start()
{
    DoAccept();
}

void CServer::DoAccept()
{
    auto &ioc = AsioIOServicePool::getInstance().GetIOService();
    auto new_session = std::make_shared<CSession>(ioc, shared_from_this());
    _acceptor.async_accept(
        new_session->GetSocket(),
        [this, new_session](const boost::system::error_code &ec)
        {
            if (!ec)
            {
                spdlog::info("[CServer] New connection accepted: {}", new_session->GetUuid());
                SessionManager::Instance().RemoveSessionByUuid(new_session->GetUuid());
                SessionManager::Instance().AddSession(0, new_session);
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
    auto old_session = SessionManager::Instance().GetSession(uid);
    if (old_session != nullptr)
    {
        spdlog::info("[CServer] User {} has existing session, closing old connection.", uid);
        old_session->Close();
    }
    SessionManager::Instance().RemoveSessionByUuid(session->GetUuid());
    SessionManager::Instance().AddSession(uid, std::move(session));
    spdlog::info("[CServer] User {} session added.", uid);
}

void CServer::RemoveUserSession(int uid)
{
    SessionManager::Instance().RemoveSession(uid);
    spdlog::info("[CServer] User {} session removed.", uid);
}

void CServer::ClearSession(const std::string &uuid)
{
    SessionManager::Instance().RemoveSessionByUuid(uuid);
    spdlog::info("[CServer] Session {} cleared.", uuid);
}

bool CServer::ForwardMessage(int target_uid, const std::string &msg_data)
{
    return MessageRouter::Instance().ForwardMessage(target_uid, msg_data);
}

void CServer::StoreOfflineMessage(int target_uid, const std::string &msg_data)
{
    auto self = shared_from_this();
    _thread_pool.Enqueue(
        [this, self, target_uid, msg_data]()
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
        });
}

void CServer::SendOfflineMessages(int uid, std::shared_ptr<CSession> session)
{
    auto self = shared_from_this();

    _thread_pool.Enqueue(
        [this, self, uid, session]()
        {
            int64_t total_count = SQLiteMgr::Instance().GetOfflineMessageCount(uid);

            if (total_count == 0)
            {
                return;
            }

            boost::asio::post(
                session->GetStrand(),
                [self, session, uid, total_count]()
                {
                    session->_offline_send_state.uid = uid;
                    session->_offline_send_state.total_count = total_count;
                    session->_offline_send_state.sent_count = 0;
                    session->_offline_send_state.sending = true;

                    session->SendNextOfflinePage();
                });
        });
}

void CServer::SetToken(int uid, const std::string &token)
{
    TokenManager::Instance().SetToken(uid, token);
}

bool CServer::CheckToken(int uid, const std::string &token)
{
    bool matched = TokenManager::Instance().CheckToken(uid, token);
    if (!matched)
    {
        spdlog::warn("[CServer] Token check failed for uid {}", uid);
    }
    return matched;
}

void CServer::RemoveToken(int uid)
{
    TokenManager::Instance().RemoveToken(uid);
}

void CServer::Stop()
{
    if (_stopped.exchange(true))
    {
        spdlog::warn("[CServer] Server is already stopping");
        return;
    }

    spdlog::info("[CServer] Stopping server, closing acceptor...");
    boost::system::error_code ec;
    _acceptor.close(ec);

    if (ec)
    {
        spdlog::error("[CServer] Failed to close acceptor: {}", ec.message());
    }
    else
    {
        spdlog::info("[CServer] Acceptor closed successfully");
    }

    _thread_pool.Shutdown();
    spdlog::info("[CServer] Server stopped");
}

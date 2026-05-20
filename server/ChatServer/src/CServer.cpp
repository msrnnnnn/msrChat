/**
 * @file CServer.cpp
 * @brief 聊天服务 TCP 入口实现
 */
#include "CServer.h"
#include "AsioIOServicePool.h"
#include "CSession.h"
#include "MessageRouter.h"
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

/**
 * @brief 启动服务器，监听端口并接受连接
 */
void CServer::Start()
{
    DoAccept();
}

/**
 * @brief 接受新连接
 * @details 递归调用以持续接受连接，关闭重复 UUID 的旧会话
 */
void CServer::DoAccept()
{
    if (_stopped) {
        return;
    }

    auto &ioc = AsioIOServicePool::getInstance().GetIOService();
    auto new_session = std::make_shared<CSession>(ioc, shared_from_this());
    _acceptor.async_accept(
        new_session->GetSocket(),
        [this, new_session](const boost::system::error_code &ec)
        {
            if (_stopped) {
                return;
            }

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

/**
 * @brief 添加用户会话
 * @param uid 用户 ID
 * @param session 会话智能指针
 * @details 踢出同一用户的旧连接，注册新会话到 SessionManager
 */
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

/**
 * @brief 移除用户会话
 * @param uid 用户 ID
 */
void CServer::RemoveUserSession(int uid)
{
    SessionManager::Instance().RemoveSession(uid);
    spdlog::info("[CServer] User {} session removed.", uid);
}

/**
 * @brief 清除指定 UUID 的会话
 * @param uuid 会话 UUID
 */
void CServer::ClearSession(const std::string &uuid)
{
    SessionManager::Instance().RemoveSessionByUuid(uuid);
    spdlog::info("[CServer] Session {} cleared.", uuid);
}

/**
 * @brief 转发消息给指定用户
 * @param target_uid 目标用户 ID
 * @param msg_data JSON 消息数据
 * @return 是否发送成功（用户不在线返回 false）
 * @details 优先实时投递，离线用户存入 SQLite
 */
bool CServer::ForwardMessage(int target_uid, const std::string &msg_data)
{
    return MessageRouter::Instance().ForwardMessage(target_uid, msg_data);
}

/**
 * @brief 转发原始协议消息
 * @param target_uid 目标用户 ID
 * @param msg_id 消息类型 ID
 * @param body_data 序列化消息体
 * @return 是否发送成功
 */
bool CServer::ForwardRawMessage(int target_uid, uint16_t msg_id, const std::string &body_data)
{
    auto session = SessionManager::Instance().GetSession(target_uid);
    if (!session)
    {
        spdlog::warn("[CServer] ForwardRawMessage: target user {} not online", target_uid);
        return false;
    }
    session->Send(body_data, msg_id);
    return true;
}

/**
 * @brief 存储离线消息
 * @param target_uid 目标用户 ID
 * @param msg_data 消息 JSON 数据
 * @return 是否存储成功
 * @details 异步写入 SQLite，消息状态置为 0
 */
bool CServer::StoreOfflineMessage(int target_uid, const std::string &msg_data)
{
    std::promise<bool> result_promise;
    auto future = result_promise.get_future();

    auto self = shared_from_this();
    _thread_pool.Enqueue(
        [this, self, target_uid, msg_data, &result_promise]()
        {
            bool success = false;
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
                success = true;
            }
            catch (const std::exception &e)
            {
                spdlog::error("[CServer] StoreOfflineMessage failed: {}", e.what());
            }
            result_promise.set_value(success);
        });

    return future.get();
}

/**
 * @brief 发送离线消息给用户
 * @param uid 用户 ID
 * @param session 目标会话
 * @details 分页拉取离线消息并发送，发送完成后清空离线记录
 */
void CServer::SendOfflineMessages(int uid, std::shared_ptr<CSession> session)
{
    auto self = shared_from_this();

    // 所有 session 访问都通过 strand，确保线程安全
    boost::asio::post(
        session->GetStrand(),
        [self, session, uid]()
        {
            int64_t total_count = SQLiteMgr::Instance().GetOfflineMessageCount(uid);

            if (total_count == 0)
            {
                return;
            }

            session->_offline_send_state.uid = uid;
            session->_offline_send_state.total_count = total_count;
            session->_offline_send_state.sent_count = 0;
            session->_offline_send_state.sending = true;

            session->SendNextOfflinePage();
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

    SessionManager::Instance().ForEachSession([](int uid, std::shared_ptr<CSession> session) {
        (void)uid;  // 未使用参数
        session->Close();
    });
    SessionManager::Instance().ClearAll();
    spdlog::info("[CServer] All sessions closed");

    _thread_pool.Shutdown();
    spdlog::info("[CServer] Server stopped");
}

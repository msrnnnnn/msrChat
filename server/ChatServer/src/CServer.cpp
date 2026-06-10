/**
 * @file CServer.cpp
 * @brief 聊天服务 TCP 入口实现
 */
#include "CServer.h"
#include "AsioIOServicePool.h"
#include "CSession.h"
#include "Message.pb.h"
#include "MessageRouter.h"
#include "SQLiteMgr.h"
#include "const.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

/**
 * @brief 构造函数
 * @param io_context Boost.Asio I/O 上下文
 * @param port 监听端口号
 * @details 初始化 acceptor 和内部线程池
 */
CServer::CServer(boost::asio::io_context &io_context, uint16_t port)
    : _io_context(io_context),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      _thread_pool(std::thread::hardware_concurrency())
{
    spdlog::info("[CServer] Server initialized on port {}", port);
}

/**
 * @brief 析构函数，关闭线程池
 */
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
                SessionManager::Instance().AddSession(-1, new_session);
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
    try
    {
        auto json_data = nlohmann::json::parse(msg_data);
        ChatMessage msg;
        msg.from_uid = json_data.value("from_uid", 0);
        msg.to_uid = target_uid;
        if (msg.from_uid <= 0 || msg.to_uid <= 0)
        {
            spdlog::warn("[CServer] StoreOfflineMessage rejected: invalid uid from={} to={}", msg.from_uid, msg.to_uid);
            return false;
        }
        msg.content = json_data.value("content", "");
        msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
        msg.status = 0;
        msg.client_msg_id = json_data.value("client_msg_id", "");
        return SQLiteMgr::Instance().SaveOfflineMessage(msg);
    }
    catch (const std::exception &e)
    {
        spdlog::error("[CServer] StoreOfflineMessage failed: {}", e.what());
        return false;
    }
}

/**
 * @brief 存储离线消息（ChatMessage 重载）
 * @param msg 构造好的 ChatMessage（支持图片等任意类型）
 * @return 是否存储成功
 */
bool CServer::StoreOfflineMessage(const ChatMessage &msg)
{
    return SQLiteMgr::Instance().SaveOfflineMessage(msg);
}

/**
 * @brief 发送离线消息给用户
 * @param uid 用户 ID
 * @param session 目标会话
 * @details 分页拉取离线消息并发送，发送完成后清空离线记录
 */
void CServer::SendOfflineMessages(int uid, const std::shared_ptr<CSession> &session)
{
    auto self = shared_from_this();

    // 所有 session 访问都通过 strand，确保线程安全
    boost::asio::post(
        session->GetStrand(),
        [self, session, uid]()
        {
            int64_t total_count = SQLiteMgr::Instance().GetOfflineMessageCount(uid);

            if (total_count > 0)
            {
                {
                    std::lock_guard<std::recursive_mutex> lock(session->_offline_mutex);
                    session->_offline_send_state.uid = uid;
                    session->_offline_send_state.total_count = total_count;
                    session->_offline_send_state.sent_count = 0;
                    session->_offline_send_state.sending = true;
                    session->_offline_send_state.last_sent_id = 0;
                }

                session->SendNextOfflinePage();
            }

            self->FlushRecallNotifies(uid, session);
        });
}

/**
 * @brief 优雅停止服务器
 * @details 原子标记防止重复调用，按顺序关闭 acceptor → 会话 → 线程池
 */
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

    // 先关闭所有会话再清理映射，防止中途被 DoAccept 加入新会话
    SessionManager::Instance().ForEachSession([](int /*uid*/, const std::shared_ptr<CSession> &session) {
        session->Close();
    });
    SessionManager::Instance().ClearAll();
    spdlog::info("[CServer] All sessions closed");

    _thread_pool.Shutdown();
    spdlog::info("[CServer] Server stopped");
}

/**
 * @brief 刷新撤回通知
 * @param uid 用户 ID
 * @param session 目标会话
 * @details 在 strand 上批量发送待投递的撤回通知，发送完毕后清空
 */
void CServer::FlushRecallNotifies(int uid, const std::shared_ptr<CSession> &session)
{
    auto self = shared_from_this();
    boost::asio::post(
        session->GetStrand(),
        [self, session, uid]()
        {
            auto entries = SQLiteMgr::Instance().PopRecallNotifies(uid);
            for (const auto &e : entries)
            {
                qmsrchat::RecallNotify n;
                n.set_msg_timestamp(e.msg_timestamp);
                n.set_recall_uid(e.recall_uid);
                n.set_recalled_to(e.recalled_to);
                n.set_recall_ts(e.recall_ts);
                std::string s;
                n.SerializeToString(&s);
                session->Send(s, MSG_CHAT_RECALL_NOTIFY);
                spdlog::info("[CServer] FlushRecallNotifies: sent 1014 to uid={} for ts={}", uid, e.msg_timestamp);
            }
            if (!entries.empty())
            {
                SQLiteMgr::Instance().ClearRecallNotifies(uid);
            }
        });
}

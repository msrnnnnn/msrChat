#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "ConfigMgr.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <iostream>
#include <memory>
#include <functional>
#include <mutex>
#include <spdlog/spdlog.h>

using namespace std;
using namespace ChatApp;

LogicSystem::LogicSystem() : _b_stop(false) {
    RegisterCallBacks();
    _worker_thread = std::thread(&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem() {
    _b_stop = true;
    _consume.notify_one();
    if (_worker_thread.joinable()) {
        _worker_thread.join();
    }
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> msg) {
    std::unique_lock<std::mutex> lock(_mutex);
    _msg_que.push(msg);
    if (_msg_que.size() == 1) {
        _consume.notify_one();
    }
}

void LogicSystem::DealMsg() {
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_msg_que.empty() && !_b_stop) {
            _consume.wait(lock);
        }
        if (_b_stop) {
            while (!_msg_que.empty()) {
                // drain queue if needed or just exit
                _msg_que.pop();
            }
            break;
        }
        
        auto msg_node = _msg_que.front();
        _msg_que.pop();
        lock.unlock();

        short msg_id = msg_node->_recvnode->_msg_id;
        auto call_back_iter = _fun_callbacks.find(msg_id);
        if (call_back_iter != _fun_callbacks.end()) {
            call_back_iter->second(msg_node->_session, msg_id, 
                std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
        }
    }
}

void LogicSystem::RegisterCallBacks() {
    _fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
        placeholders::_1, placeholders::_2, placeholders::_3);
    _fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriendApplyHandler, this,
        placeholders::_1, placeholders::_2, placeholders::_3);
    _fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendHandler, this,
        placeholders::_1, placeholders::_2, placeholders::_3);
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data, root);
    
    spdlog::info("LoginHandler called. MsgData: {}", msg_data);

    // 提前准备回包，确保任何路径都能回复客户端
    Json::Value rtvalue;
    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, MSG_CHAT_LOGIN_RSP);
    });

    if (!root.isMember("uid") || !root.isMember("token")) {
        spdlog::error("LoginHandler error: uid or token missing in JSON");
        rtvalue["error"] = (int)ErrorCode::Error_Json;
        return;
    }

    auto uid = root["uid"].asInt();
    auto token = root["token"].asString();
    
    spdlog::info("user login uid: {}, token: {}", uid, token);
    
    //从状态服务器获取token匹配是否准确 
    auto rsp = StatusGrpcClient::GetInstance()->Login(uid, token);
    
    rtvalue["error"] = rsp.error();
    if (rsp.error() != (int)ErrorCode::Success) {
        spdlog::error("StatusServer Login failed. Error: {}", rsp.error());
        return;
    }

    //内存中查询用户信息 
    auto find_iter = _users.find(uid);
    std::shared_ptr<UserInfo> user_info = nullptr;
    if (find_iter == _users.end()) {
        //查询数据库 
        user_info = MysqlMgr::GetInstance()->GetUser(uid);
        if (user_info == nullptr) {
            spdlog::error("MysqlMgr GetUser failed for uid: {}", uid);
            rtvalue["error"] = (int)ErrorCode::UidInvalid;
            return;
        }

        _users[uid] = user_info;
    }
    else {
        user_info = find_iter->second;
    }

    // 保存session
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _user_sessions[uid] = session;
    }

    rtvalue["uid"] = uid;
    rtvalue["token"] = rsp.token();
    rtvalue["name"] = user_info->name;
    spdlog::info("Login success for uid: {}, name: {}", uid, user_info->name);
}

void LogicSystem::AddFriendApplyHandler(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data, root);

    int applyuid = root["applyuid"].asInt();
    int touid = root["touid"].asInt();
    std::string name = root["name"].asString();
    std::string back = root["back"].asString();

    Json::Value rtvalue;
    rtvalue["error"] = (int)ErrorCode::Success;
    rtvalue["applyuid"] = applyuid;
    rtvalue["touid"] = touid;

    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, ID_ADD_FRIEND_RSP);
    });

    // 写入数据库
    if (!MysqlMgr::GetInstance()->AddFriendApply(applyuid, touid, back)) {
        rtvalue["error"] = (int)ErrorCode::Error_Json; // Or other error
        return;
    }

    // 查找目标用户是否在线
    std::lock_guard<std::mutex> lock(_mutex);
    auto find_iter = _user_sessions.find(touid);
    if (find_iter != _user_sessions.end()) {
        auto target_session = find_iter->second;
        Json::Value notify;
        notify["applyuid"] = applyuid;
        notify["name"] = name;
        notify["desc"] = back;
        
        // 获取申请人详细信息
        auto user_info = _users.find(applyuid);
        if (user_info != _users.end()) {
             notify["icon"] = user_info->second->icon;
             notify["nick"] = user_info->second->nick;
             notify["sex"] = user_info->second->sex;
        } else {
             // 从DB加载
             auto u_info = MysqlMgr::GetInstance()->GetUser(applyuid);
             if (u_info) {
                 notify["icon"] = u_info->icon;
                 notify["nick"] = u_info->nick;
                 notify["sex"] = u_info->sex;
                 _users[applyuid] = u_info;
             }
        }
        notify["status"] = 0;
        
        std::string notify_str = notify.toStyledString();
        target_session->Send(notify_str, ID_NOTIFY_ADD_FRIEND_REQ);
    }
}

void LogicSystem::AuthFriendHandler(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data, root);

    int uid = root["uid"].asInt(); // 当前用户 (auth user)
    int touid = root["touid"].asInt(); // 申请人 (applicant)

    Json::Value rtvalue;
    rtvalue["error"] = (int)ErrorCode::Success;
    rtvalue["uid"] = uid;
    rtvalue["touid"] = touid;

    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, ID_AUTH_FRIEND_RSP);
    });

    // 更新数据库
    if (!MysqlMgr::GetInstance()->AuthFriendApply(touid, uid)) {
        rtvalue["error"] = (int)ErrorCode::Error_Json;
        return;
    }
    
    // 双向添加好友
    MysqlMgr::GetInstance()->AddFriend(uid, touid, "");
    MysqlMgr::GetInstance()->AddFriend(touid, uid, "");

    // 通知申请人
    std::lock_guard<std::mutex> lock(_mutex);
    auto find_iter = _user_sessions.find(touid);
    if (find_iter != _user_sessions.end()) {
        auto target_session = find_iter->second;
        Json::Value notify;
        notify["uid"] = uid;
        
        auto user_info = _users.find(uid);
        if (user_info != _users.end()) {
             notify["name"] = user_info->second->name;
             notify["nick"] = user_info->second->nick;
             notify["icon"] = user_info->second->icon;
             notify["sex"] = user_info->second->sex;
        } else {
             auto u_info = MysqlMgr::GetInstance()->GetUser(uid);
             if (u_info) {
                 notify["name"] = u_info->name;
                 notify["nick"] = u_info->nick;
                 notify["icon"] = u_info->icon;
                 notify["sex"] = u_info->sex;
                 _users[uid] = u_info;
             }
        }
        
        std::string notify_str = notify.toStyledString();
        target_session->Send(notify_str, ID_NOTIFY_AUTH_FRIEND_REQ);
    }
}

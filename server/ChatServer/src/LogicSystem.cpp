#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <iostream>

using namespace std;
using namespace ChatApp;

LogicSystem::LogicSystem() : _b_stop(false) {
    RegisterCallBacks();
    _worker_thread = std::thread(&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem() {
    _b_stop = true;
    _consume.notify_one();
    _worker_thread.join();
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> msg) {
    std::unique_lock<std::mutex> unique_lk(_mutex);
    _msg_que.push(msg);
    // 由0变为1则发送通知
    if (_msg_que.size() == 1) {
        _consume.notify_one();
    }
}

void LogicSystem::DealMsg() {
    while (true) {
        std::unique_lock<std::mutex> unique_lk(_mutex);
        // 判断队列为空则阻塞
        while (_msg_que.empty() && !_b_stop) {
            _consume.wait(unique_lk);
        }

        // 判断是否为关闭状态
        if (_b_stop) {
            while (!_msg_que.empty()) {
                auto msg_node = _msg_que.front();
                // can do some resource release here
                _msg_que.pop();
            }
            break;
        }

        // 如果没有关闭，则取出数据
        auto msg_node = _msg_que.front();
        _msg_que.pop();
        unique_lk.unlock();

        short msg_id = msg_node->_recvnode->_msg_id;
        auto call_back_iter = _fun_callbacks.find(msg_id);
        if (call_back_iter != _fun_callbacks.end()) {
            call_back_iter->second(msg_node->_session, msg_id, std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
        }
    }
}

void LogicSystem::RegisterCallBacks() {
    _fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
        placeholders::_1, placeholders::_2, placeholders::_3);
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data, root);
    auto uid = root["uid"].asInt();
    std::cout << "user login uid is  " << uid << " user token  is "
        << root["token"].asString() << endl;
    
    //从状态服务器获取token匹配是否准确 
    auto rsp = StatusGrpcClient::GetInstance()->Login(uid, root["token"].asString());
    Json::Value  rtvalue;
    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, MSG_CHAT_LOGIN_RSP);
    });

    rtvalue["error"] = rsp.error();
    if (rsp.error() != (int)ErrorCode::Success) {
        return;
    }

    //内存中查询用户信息 
    auto find_iter = _users.find(uid);
    std::shared_ptr<UserInfo> user_info = nullptr;
    if (find_iter == _users.end()) {
        //查询数据库 
        user_info = MysqlMgr::GetInstance()->GetUser(uid);
        if (user_info == nullptr) {
            rtvalue["error"] = (int)ErrorCode::UidInvalid;
            return;
        }

        _users[uid] = user_info;
    }
    else {
        user_info = find_iter->second;
    }

    rtvalue["uid"] = uid;
    rtvalue["token"] = rsp.token();
    rtvalue["name"] = user_info->name;
}

#pragma once
#include "Singleton.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <functional>
#include "CSession.h"
#include "MsgNode.h"
#include "MysqlMgr.h"

class LogicNode {
public:
    LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<RecvNode> recvnode) 
        : _session(session), _recvnode(recvnode) {}
    std::shared_ptr<CSession> _session;
    std::shared_ptr<RecvNode> _recvnode;
};

typedef std::function<void(std::shared_ptr<CSession>, const short&, const std::string&)> FunCallBack;

class LogicSystem : public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    void PostMsgToQue(std::shared_ptr<LogicNode> msg);
private:
    LogicSystem();
    void DealMsg();
    void RegisterCallBacks();
    void LoginHandler(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data);

    std::queue<std::shared_ptr<LogicNode>> _msg_que;
    std::mutex _mutex;
    std::condition_variable _consume;
    bool _b_stop;
    std::thread _worker_thread;
    std::map<short, FunCallBack> _fun_callbacks;
    std::map<int, std::shared_ptr<UserInfo>> _users;
};

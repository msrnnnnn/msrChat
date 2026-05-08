/**
 * @file TcpMgr.h
 * @brief TCP 长连接管理类
 * @details 负责与 ChatServer 建立 TCP 连接，处理数据的收发与协议解析。
 *          包含工业级心跳保活与指数退避重连机制。
 */
#ifndef TCPMGR_H
#define TCPMGR_H

#include "ProtocolStructs.h"
#include "Global.h"
#include <QMutex>
#include <QObject>
#include <QThread>

class TcpWorker;

class TcpMgr : public QObject
{
    Q_OBJECT

public:
    static TcpMgr *Instance();
    static void Init();
    static void Destroy();
    ~TcpMgr();

    void slot_tcp_connect(ServerInfo si);
    void slot_send_data(RequestType reqId, const QByteArray &data);

    void slot_send_login_req(const LoginReqStruct &req);
    void slot_send_chat_login_req(const ChatLoginReqStruct &req);
    void slot_send_chat_text_req(const ChatTextReqStruct &req);
    void slot_send_verify_code_req(const VerifyCodeReqStruct &req);
    void slot_send_register_req(const RegisterReqStruct &req);
    void slot_send_reset_pwd_req(const ResetPwdReqStruct &req);
    void slot_send_offline_ack_req(const OfflineAckReqStruct &req);

    struct FileReqStruct
    {
        int64_t task_id;
        int from_uid;
        int to_uid;
        QString filename;
        int64_t total_size;
        QString md5;
    };

    void slot_send_file_req(const FileReqStruct &req);

signals:
    void sig_con_success(bool bsuccess);
    void sig_login_failed(int err);
    void sig_reconnected();

    void sig_login_rsp(const LoginRspStruct &rsp);
    void sig_chat_login_rsp(const ChatLoginRspStruct &rsp);
    void sig_chat_text_msg(const ChatTextMsgStruct &msg);
    void sig_chat_ack(const ChatAckStruct &ack);
    void sig_offline_ack(const OfflineAckStruct &ack);
    void sig_verify_code_rsp(const VerifyCodeRspStruct &rsp);
    void sig_register_rsp(const RegisterRspStruct &rsp);
    void sig_reset_pwd_rsp(const ResetPwdRspStruct &rsp);

private slots:
    void slot_parse_login_rsp(RequestType req_type, const QByteArray &data);
    void slot_parse_chat_login_rsp(quint16 msg_id, const QByteArray &data);
    void slot_parse_chat_msg(quint16 msg_id, const QByteArray &data);

private:
    explicit TcpMgr(QObject *parent = nullptr);
    void init_thread();

    static QMutex _mutex;
    static TcpMgr *_instance;

    QThread *_netThread;
    TcpWorker *_worker;
};

#endif

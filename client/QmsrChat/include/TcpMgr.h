#pragma once
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
#include "TcpProtocolParser.h"
#include <QObject>
#include <QThread>

class TcpWorker;

/**
 * @brief TCP 长连接管理类（单例）
 * @details 管理 TCP 连接生命周期，提供高层业务接口用于发送各类协议请求。
 *          内部维护工作线程、协议解析器和连接状态。信号用于向 UI 层转发服务端响应。
 */
class TcpMgr : public QObject
{
    Q_OBJECT

public:
    static TcpMgr *Instance();
    static void Init();
    static void Destroy();
    ~TcpMgr();

    bool IsConnected() const;

    /**
     * @brief 发起 TCP 连接
     * @param si 服务器地址信息
     */
    void slotTcpConnect(ServerInfo si);
    /**
     * @brief 发送原始协议数据
     * @param reqId 请求类型
     * @param data 序列化后的协议数据
     */
    void slotSendData(RequestType reqId, const QByteArray &data);

    void slot_send_login_req(const LoginReqStruct &req);
    void slot_send_chat_login_req(const ChatLoginReqStruct &req);
    void slot_send_chat_text_req(const ChatTextReqStruct &req);
    void slot_send_verify_code_req(const VerifyCodeReqStruct &req);
    void slot_send_register_req(const RegisterReqStruct &req);
    void slot_send_reset_pwd_req(const ResetPwdReqStruct &req);
    /**
     * @brief 发送离线消息确认
     * @param req 离线确认请求结构体
     */
    void slot_send_offline_ack_req(const OfflineAckReqStruct &req);

    /**
     * @brief 发送文件传输请求
     * @param req 文件请求结构体（含分片数据）
     */
    void slot_send_file_req(const FileReqStruct &req);

    void slot_send_chat_recall(const ChatRecallMsgStruct &req);
    void slot_send_chat_edit(const ChatEditMsgStruct &req);

    void slot_send_chat_image(const ChatImageStruct &msg);
    /**
     * @brief 请求下载图片
     * @param image_id 图片唯一标识
     */
    void slot_send_image_download_req(const QString &image_id);

signals:
    void sigConSuccess(bool bsuccess);
    /**
     * @brief 断线重连成功后发射
     */
    void sigReconnected();

    void sigLoginRsp(const LoginRspStruct &rsp);
    void sigChatLoginRsp(const ChatLoginRspStruct &rsp);
    void sigChatTextMsg(const ChatTextMsgStruct &msg);
    void sigChatAck(const ChatAckStruct &ack);
    /**
     * @brief 离线消息确认通知
     */
    void sigOfflineAck(const OfflineAckStruct &ack);
    void sigVerifyCodeRsp(const VerifyCodeRspStruct &rsp);
    void sigRegisterRsp(const RegisterRspStruct &rsp);
    void sigResetPwdRsp(const ResetPwdRspStruct &rsp);

    void sigChatImage(const ChatImageStruct &msg);
    void sigImageDownloadRsp(const ImageDownloadRspStruct &rsp);
    void sigChatRecallRsp(const ChatEditAckStruct &ack);
    void sigChatEditAck(const ChatEditAckStruct &ack);
    /**
     * @brief 消息撤回通知（他人撤回消息时触发）
     */
    void sigChatRecallNotify(const ChatRecallNotifyStruct &n);
    /**
     * @brief 消息编辑通知（他人编辑消息时触发）
     */
    void sigChatEditNotify(const ChatEditNotifyStruct &n);

    /**
     * @brief 通知工作线程停止运行
     */
    void sigStopWorker();
    void sigConnectWorker(ServerInfo si);
    void sigSendDataWorker(RequestType reqId, const QByteArray &data);

private slots:
    /**
     * @brief 解析完成后分发协议包到对应处理函数
     * @param msg_id 消息类型 ID
     * @param data 消息体数据
     */
    void slotDispatchPacket(quint16 msg_id, const QByteArray &data);

private:
    explicit TcpMgr(QObject *parent = nullptr);
    /**
     * @brief 初始化网络工作线程
     */
    void init_thread();
    /**
     * @brief 处理文件传输协议包（含分片组装逻辑）
     * @param req_type 请求类型
     * @param data 文件分片数据
     */
    void handle_file_packet(RequestType req_type, const QByteArray &data);

    static TcpMgr *_instance;

    QThread *_netThread;
    TcpWorker *_worker;
    TcpProtocolParser _parser{*this};
    std::atomic<bool> _is_connected{false};
};

#endif

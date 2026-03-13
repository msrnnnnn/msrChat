/**
 * @file tcpmgr.h
 * @brief TCP 长连接管理类
 * @details 负责与 ChatServer 建立 TCP 连接，处理数据的收发与协议解析。
 *          包含工业级心跳保活与指数退避重连机制。
 */
#ifndef TCPMGR_H
#define TCPMGR_H

#include "global.h"
#include <QObject>
#include <QMutex>
#include <QThread>

class TcpWorker;

/**
 * @class TcpMgr
 * @brief TCP 网络管理单例类
 * @details 管理 QTcpSocket 对象，处理 socket 状态及数据收发。
 *          支持心跳保活和自动重连功能。
 */
class TcpMgr : public QObject
{
    Q_OBJECT

public:
    static TcpMgr *GetInstance();
    static void DestroyInstance();
    ~TcpMgr();

private:
    explicit TcpMgr(QObject *parent = nullptr);
    void init_thread();

    static QMutex _mutex;
    static TcpMgr *_instance;

    QThread *_netThread;
    TcpWorker *_worker;

public slots:
    /**
     * @brief 连接服务器槽函数
     * @param si 服务器信息结构体 (IP, Port, Token, Uid)
     */
    void slot_tcp_connect(ServerInfo si);

    /**
     * @brief 发送数据槽函数
     * @param reqId 请求类型 ID
     * @param data  发送的数据内容 (JSON 字符串)
     */
    void slot_send_data(RequestType reqId, const QString &data);

signals:
    /**
     * @brief 连接成功/失败信号
     * @param bsuccess true 表示连接成功，false 表示失败
     */
    void sig_con_success(bool bsuccess);

    /**
     * @brief 数据发送信号 (通常用于调试或日志)
     * @param reqId 请求 ID
     * @param data  发送的数据
     */
    void sig_send_data(RequestType reqId, QString data);

    /**
     * @brief 登录失败信号 (TCP 连接建立后的业务登录)
     * @param err 错误码
     */
    void sig_login_failed(int err);

    /**
     * @brief 收到完整消息信号
     * @param reqId 消息类型 ID
     * @param data  消息体数据
     */
    void sig_msg_received(RequestType reqId, QByteArray data);

    /**
     * @brief 收到消息信号 (quint16 版本)
     * @param msg_id 消息类型 ID
     * @param data   消息体数据
     */
    void sig_msg_received(quint16 msg_id, QByteArray data);

    /**
     * @brief 重连成功信号
     * @details 当断线后重新连接成功时发出，通知业务层重新发送登录包
     */
    void sig_reconnected();
};

#endif

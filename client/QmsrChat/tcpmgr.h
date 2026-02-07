/**
 * @file tcpmgr.h
 * @brief TCP 连接管理类
 * @author msr
 */

#ifndef TCPMGR_H
#define TCPMGR_H

#include "Singleton.h"
#include "global.h"
#include <QMap>
#include <QObject>
#include <QTcpSocket>
#include <functional>
#include <memory>

/**
 * @class TcpMgr
 * @brief TCP 连接管理器
 * @details 管理与聊天服务器的长连接，处理粘包、拆包及数据收发。
 */
class TcpMgr : public QObject, public Singleton<TcpMgr>, public std::enable_shared_from_this<TcpMgr>
{
    Q_OBJECT
public:
    friend class Singleton<TcpMgr>;
    ~TcpMgr();

private:
    TcpMgr();
    void initHandlers();
    void handleMsg(ReqId id, int len, QByteArray data);

    QTcpSocket _socket;
    QString _host;
    uint16_t _port;
    QByteArray _buffer;
    bool _b_recv_pending;
    quint16 _message_id;
    quint16 _message_len;
    QMap<ReqId, std::function<void(ReqId id, int len, QByteArray data)>> _handlers;

public slots:
    /**
     * @brief 连接到聊天服务器
     * @param si 服务器信息
     */
    void slot_tcp_connect(ServerInfo si);

    /**
     * @brief 发送数据
     * @param reqId 请求ID
     * @param data 数据内容
     */
    void slot_send_data(ReqId reqId, QString data);

signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(ReqId reqId, QString data);
    void sig_swich_chatdlg();
    void sig_login_failed(int err);
};

#endif // TCPMGR_H

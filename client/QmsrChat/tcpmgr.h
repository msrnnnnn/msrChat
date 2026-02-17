#ifndef TCPMGR_H
#define TCPMGR_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include "singleton.h"
#include "global.h"

class TcpMgr : public QObject, public Singleton<TcpMgr>
{
    Q_OBJECT
    friend class Singleton<TcpMgr>;

public:
    ~TcpMgr();

private:
    TcpMgr(); // 私有构造

    QTcpSocket _socket;
    QString _host;
    uint16_t _port;

    QByteArray _buffer;     // 核心：接收缓冲区

    bool _b_head_parsed;

    quint16 _message_id;    // 核心：当前正在解析的消息ID
    quint16 _message_len;   // 核心：当前正在解析的消息长度

public slots:
    void slot_tcp_connect(const QString& ip, quint16 port);
    void slot_send_data(RequestType reqId, QString data);

signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(RequestType reqId, QString data);
    void sig_login_failed(int err);
    // 补充：通常会有收到数据的信号，这里我加上，方便你逻辑层使用
    void sig_msg_received(RequestType reqId, QByteArray data);
};

#endif // TCPMGR_H

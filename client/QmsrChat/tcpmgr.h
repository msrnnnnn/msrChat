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
    TcpMgr();

    QTcpSocket _socket;
    QString _host;
    uint16_t _port;

    QByteArray _buffer;
    bool _b_head_parsed;
    quint16 _message_id;
    quint16 _message_len;

public slots:
    void slot_tcp_connect(ServerInfo si);
    void slot_send_data(RequestType reqId, QString data);

signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(RequestType reqId, QString data);
    void sig_login_failed(int err);
    void sig_msg_received(RequestType reqId, QByteArray data);
};

#endif // TCPMGR_H

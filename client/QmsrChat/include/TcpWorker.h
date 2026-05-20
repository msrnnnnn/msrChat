/**
 * @file TcpWorker.h
 * @brief TCP 工作线程类
 * @details 封装 QTcpSocket 在线程内的连接、收发、心跳与重连逻辑。
 */
#ifndef TCPWORKER_H
#define TCPWORKER_H

#include "RingBuffer.h"
#include "Global.h"
#include <QByteArray>
#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <optional>

class TcpWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpWorker(QObject *parent = nullptr);
    ~TcpWorker();

public slots:
    void slot_init();
    void slot_tcp_connect(ServerInfo si);
    void slot_send_data(RequestType reqId, const QByteArray &data);
    void slot_stop();

signals:
    void sig_con_success(bool bsuccess);
    void sig_packet_received(quint16 msg_id, QByteArray data);
    void sig_reconnected();

private slots:
    void slot_ready_read();
    void slot_connected();
    void slot_error(QAbstractSocket::SocketError error);
    void slot_disconnected();
    void slot_send_ping();
    void slot_pong_check();
    void slot_reconnect_timeout();

private:
    enum class ConnectionState
    {
        Idle,
        Connecting,
        Connected,
        Reconnecting,
        Stopping
    };

    QByteArray readBytes(qsizetype len);
    void schedule_reconnect();
    void reset_buffer();
    void stop_timers();
    bool can_send() const;

    QTcpSocket *_socket;
    QString _host;
    uint16_t _port;
    QMutex _pending_connect_mutex;
    mutable QMutex _host_port_mutex;
    std::optional<ServerInfo> _pending_connect;

    RingBuffer _recv_buffer;
    bool _b_head_parsed;
    quint16 _message_id;
    quint32 _message_len;

    QTimer *_heartbeat_timer;
    QTimer *_pong_check_timer;
    QTimer *_reconnect_timer;
    int _reconnect_interval;
    qint64 _last_pong_time;
    std::atomic<ConnectionState> _state;

    static const quint32 MAX_MESSAGE_LEN = 1024 * 1024;
};

#endif

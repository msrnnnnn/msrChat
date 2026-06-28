#pragma once
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

/**
 * @brief TCP 工作线程类
 * @details 运行于独立 QThread 中，封装 QTcpSocket 的底层操作。
 *          负责连接管理、二进制协议组帧、心跳保活、指数退避重连。
 */
class TcpWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpWorker(QObject *parent = nullptr);
    ~TcpWorker();

public slots:
    void slot_init();
    void slotTcpConnect(ServerInfo si);
    void slotSendData(RequestType reqId, const QByteArray &data);
    void slot_stop();

signals:
    void sigConSuccess(bool bsuccess);
    void sigPacketReceived(quint16 msg_id, QByteArray data);
    /**
     * @brief 断线重连成功后发射
     */
    void sigReconnected();

private slots:
    void slot_ready_read();
    void slot_connected();
    void slot_error(QAbstractSocket::SocketError error);
    void slot_disconnected();
    void slot_send_ping();
    /**
     * @brief 检查最近一次 pong 是否超时，超时则触发重连
     */
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

    /**
     * @brief 从 socket 读取指定字节数
     * @param len 需要读取的字节数
     * @return 读取到的数据
     */
    QByteArray readBytes(qsizetype len);
    /**
     * @brief 启动指数退避重连定时器
     */
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
    bool _head_parsed;
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

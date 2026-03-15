/**
 * @file tcpworker.h
 * @brief TCP 工作线程类
 * @details 封装 QTcpSocket 在线程内的连接、收发、心跳与重连逻辑。
 */
#ifndef TCPWORKER_H
#define TCPWORKER_H

#include "global.h"
#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <deque>

class TcpWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpWorker(QObject *parent = nullptr);
    ~TcpWorker();

public slots:
    /**
     * @brief 初始化网络对象与信号连接
     */
    void slot_init();
    /**
     * @brief 建立 TCP 连接
     * @param si 服务器信息
     */
    void slot_tcp_connect(ServerInfo si);
    /**
     * @brief 发送协议数据
     * @param reqId 请求类型
     * @param data  JSON 字符串
     */
    void slot_send_data(RequestType reqId, const QString &data);
    /**
     * @brief 停止工作线程中的网络活动
     */
    void slot_stop();

signals:
    void sig_con_success(bool bsuccess);
    void sig_msg_received(RequestType reqId, QByteArray data);
    void sig_msg_received(quint16 msg_id, QByteArray data);
    void sig_reconnected();

private slots:
    /**
     * @brief 读取并解析到达的 TCP 数据
     */
    void slot_ready_read();
    /**
     * @brief 连接成功回调
     */
    void slot_connected();
    /**
     * @brief Socket 错误回调
     * @param error Qt Socket 错误码
     */
    void slot_error(QAbstractSocket::SocketError error);
    /**
     * @brief 断开连接回调
     */
    void slot_disconnected();
    /**
     * @brief 发送心跳包
     */
    void slot_send_ping();
    /**
     * @brief 心跳超时检查
     */
    void slot_pong_check();
    /**
     * @brief 重连定时器触发回调
     */
    void slot_reconnect_timeout();

private:
    struct BufferChunk
    {
        QByteArray data;
        int offset = 0;
    };

    bool peekBytes(qsizetype offset, char *dest, qsizetype len) const;
    void consumeBytes(qsizetype len);
    QByteArray readBytes(qsizetype len);
    void schedule_reconnect();
    void reset_buffer();

    QTcpSocket *_socket;
    QString _host;
    uint16_t _port;
    ServerInfo _pending_connect;
    bool _has_pending_connect;

    std::deque<BufferChunk> _chunks;
    qsizetype _buffered_bytes;
    bool _b_head_parsed;
    quint16 _message_id;
    quint32 _message_len;

    QTimer *_heartbeat_timer;
    QTimer *_pong_check_timer;
    QTimer *_reconnect_timer;
    int _reconnect_interval;
    bool _is_first_connection;
    qint64 _last_pong_time;

    static const quint32 MAX_MESSAGE_LEN = 1024 * 1024;
};

#endif

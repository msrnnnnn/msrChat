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
    void slot_init();
    void slot_tcp_connect(ServerInfo si);
    void slot_send_data(RequestType reqId, const QString &data);
    void slot_stop();

signals:
    void sig_con_success(bool bsuccess);
    void sig_msg_received(RequestType reqId, QByteArray data);
    void sig_msg_received(quint16 msg_id, QByteArray data);
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

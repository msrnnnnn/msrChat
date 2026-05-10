/**
 * @file tcpworker.cpp
 * @brief TCP 工作线程实现
 * @details 负责长连接生命周期、协议解包、心跳检测与自动重连。
 */
#include "TcpWorker.h"
#include <QAbstractSocket>
#include <QDataStream>
#include <QDebug>
#include <QtEndian>
#include <cstring>

TcpWorker::TcpWorker(QObject *parent)
    : QObject(parent),
      _socket(nullptr),
      _host(""),
      _port(0),
      _pending_connect(std::nullopt),
      _recv_buffer(RingBuffer(2 * 1024 * 1024)),
      _b_head_parsed(false),
      _message_id(0),
      _message_len(0),
      _heartbeat_timer(nullptr),
      _pong_check_timer(nullptr),
      _reconnect_timer(nullptr),
      _reconnect_interval(3000),
      _last_pong_time(0),
      _state(ConnectionState::Idle)
{
}

TcpWorker::~TcpWorker()
{
    // slot_stop() 已统一由 TcpMgr::~TcpMgr() 通过 BlockingQueuedConnection 调用
    // 此处不再重复调用，防止 socket/timers 在 Qt 全局清理阶段被二次操作
}

void TcpWorker::slot_init()
{
    if (_socket)
    {
        return;
    }

    _socket = new QTcpSocket(this);
    _heartbeat_timer = new QTimer(this);
    _pong_check_timer = new QTimer(this);
    _reconnect_timer = new QTimer(this);
    _reconnect_timer->setSingleShot(true);

    connect(_socket, &QTcpSocket::connected, this, &TcpWorker::slot_connected);
    connect(_socket, &QTcpSocket::readyRead, this, &TcpWorker::slot_ready_read);
    connect(_socket, &QTcpSocket::errorOccurred, this, &TcpWorker::slot_error);
    connect(_socket, &QTcpSocket::disconnected, this, &TcpWorker::slot_disconnected);
    connect(_heartbeat_timer, &QTimer::timeout, this, &TcpWorker::slot_send_ping);
    connect(_pong_check_timer, &QTimer::timeout, this, &TcpWorker::slot_pong_check);
    connect(_reconnect_timer, &QTimer::timeout, this, &TcpWorker::slot_reconnect_timeout);

    if (_pending_connect.has_value())
    {
        const ServerInfo si = *_pending_connect;
        _pending_connect.reset();
        slot_tcp_connect(si);
    }
}

void TcpWorker::slot_stop()
{
    _state = ConnectionState::Stopping;
    _pending_connect.reset();
    stop_timers();
    if (_socket)
    {
        _socket->abort();
    }
    reset_buffer();
}

void TcpWorker::slot_tcp_connect(ServerInfo si)
{
    if (!_socket)
    {
        _pending_connect = si;
        return;
    }

    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _pending_connect = si;
    _reconnect_interval = 3000;
    _last_pong_time = 0;
    stop_timers();
    reset_buffer();

    _socket->abort();
    _state = ConnectionState::Connecting;
    _socket->connectToHost(_host, _port);
}

void TcpWorker::slot_send_data(RequestType reqId, const QByteArray &data)
{
    if (!_socket)
    {
        qWarning() << "Tcp send rejected: socket not initialized";
        return;
    }
    if (!can_send())
    {
        qWarning() << "Tcp send rejected: connection state is not Connected";
        return;
    }

    const auto id = static_cast<quint16>(reqId);
    const auto len = static_cast<quint32>(data.size());

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << id << len;
    block.append(data);

    const qint64 written = _socket->write(block);
    if (written != block.size())
    {
        qWarning() << "Tcp send failed: expected" << block.size() << "bytes, wrote" << written;
        return;
    }
    qDebug() << "Tcp Send: ID=" << id << " Len=" << len;
}

void TcpWorker::slot_connected()
{
    const bool was_reconnecting = _state == ConnectionState::Reconnecting;

    if (_reconnect_timer->isActive())
    {
        _reconnect_timer->stop();
    }
    _reconnect_interval = 3000;
    _last_pong_time = QDateTime::currentMSecsSinceEpoch();
    _state = ConnectionState::Connected;

    _heartbeat_timer->start(15000);
    _pong_check_timer->start(5000);

    if (was_reconnecting)
    {
        emit sig_reconnected();
    }

    emit sig_con_success(true);
}

void TcpWorker::slot_ready_read()
{
    if (!_socket)
    {
        return;
    }

    QByteArray data = _socket->readAll();
    if (!data.isEmpty())
    {
        std::size_t data_size = static_cast<std::size_t>(data.size());
        if (data_size > RingBuffer::kMaxCapacity)
        {
            qWarning() << "SECURITY: Incoming data size" << data_size << "exceeds buffer max capacity"
                       << RingBuffer::kMaxCapacity << ". Possible malicious packet.";
            _socket->abort();
            return;
        }

        if (!_recv_buffer.Write(data.constData(), data_size))
        {
            qWarning() << "SECURITY: Failed to write" << data_size << "bytes to buffer. Buffer capacity exhausted.";
            _socket->abort();
            return;
        }
    }

    while (true)
    {
        if (!_b_head_parsed)
        {
            if (_recv_buffer.Available() < 6)
            {
                break;
            }

            char header[6];
            if (_recv_buffer.Read(header, 6) < 6)
            {
                break;
            }

            _message_id = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(header));
            _message_len = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(header + 2));

            if (_message_len > MAX_MESSAGE_LEN)
            {
                qWarning() << "SECURITY: Malicious packet detected! Message length" << _message_len << "exceeds limit"
                           << MAX_MESSAGE_LEN;
                _socket->abort();
                return;
            }

            if (_message_len == 0)
            {
                qWarning() << "WARNING: Empty message received. Closing connection.";
                _recv_buffer.Clear();
                _b_head_parsed = false;
                _message_len = 0;
                _socket->disconnectFromHost();
                return;
            }

            _b_head_parsed = true;
        }

        if (_b_head_parsed)
        {
            if (_recv_buffer.Available() < static_cast<std::size_t>(_message_len))
            {
                break;
            }

            QByteArray messageBody = readBytes(static_cast<qsizetype>(_message_len));
            qDebug() << "Recv Packet: ID=" << _message_id << " Len=" << _message_len;

            if (_message_id == static_cast<quint16>(RequestType::MSG_HELLO))
            {
                _last_pong_time = QDateTime::currentMSecsSinceEpoch();
                qDebug() << "Pong received, updated _last_pong_time";
            }
            else
            {
                qDebug() << "Recv message ID=" << _message_id << " forwarded to TcpMgr for parsing";
                emit sig_packet_received(_message_id, messageBody);
            }

            _b_head_parsed = false;
        }
    }
}

void TcpWorker::slot_error(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)

    if (_state == ConnectionState::Stopping)
    {
        return;
    }

    stop_timers();
    emit sig_con_success(false);
    schedule_reconnect();
}

void TcpWorker::slot_disconnected()
{
    if (_state == ConnectionState::Stopping)
    {
        return;
    }

    stop_timers();
    emit sig_con_success(false);
    schedule_reconnect();
}

void TcpWorker::slot_send_ping()
{
    slot_send_data(RequestType::MSG_HELLO, "{}");
}

void TcpWorker::slot_pong_check()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - _last_pong_time;

    if (_last_pong_time > 0 && elapsed > 45000)
    {
        if (_socket)
        {
            _socket->disconnectFromHost();
        }
    }
}

void TcpWorker::slot_reconnect_timeout()
{
    if (!_socket || _state != ConnectionState::Reconnecting)
    {
        return;
    }
    if (_host.isEmpty() || _port == 0)
    {
        qWarning() << "Reconnect skipped: missing target host or port";
        _state = ConnectionState::Idle;
        return;
    }

    _state = ConnectionState::Connecting;
    if (_socket)
    {
        _socket->connectToHost(_host, _port);
    }
}

QByteArray TcpWorker::readBytes(qsizetype len)
{
    QByteArray result;
    if (len <= 0)
    {
        return result;
    }

    result.resize(len);
    std::size_t bytesRead = _recv_buffer.Read(result.data(), static_cast<std::size_t>(len));
    if (bytesRead < static_cast<std::size_t>(len))
    {
        result.resize(static_cast<int>(bytesRead));
    }

    return result;
}

void TcpWorker::schedule_reconnect()
{
    if (!_socket || _state == ConnectionState::Stopping)
    {
        return;
    }
    if (_host.isEmpty() || _port == 0)
    {
        _state = ConnectionState::Idle;
        return;
    }
    if (_reconnect_timer->isActive())
    {
        return;
    }
    if (_socket->state() != QAbstractSocket::UnconnectedState)
    {
        _socket->abort();
    }

    _state = ConnectionState::Reconnecting;
    _reconnect_timer->start(_reconnect_interval);
    _reconnect_interval = (std::min)(_reconnect_interval * 2, 60000);
}

void TcpWorker::reset_buffer()
{
    _recv_buffer.Clear();
    _b_head_parsed = false;
    _message_id = 0;
    _message_len = 0;
}

void TcpWorker::stop_timers()
{
    if (_heartbeat_timer)
    {
        _heartbeat_timer->stop();
    }
    if (_pong_check_timer)
    {
        _pong_check_timer->stop();
    }
    if (_reconnect_timer)
    {
        _reconnect_timer->stop();
    }
}

bool TcpWorker::can_send() const
{
    return _state == ConnectionState::Connected && _socket && _socket->state() == QAbstractSocket::ConnectedState;
}

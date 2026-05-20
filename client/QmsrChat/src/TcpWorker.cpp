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

// 心跳与重连配置常量
constexpr int HEARTBEAT_INTERVAL_MS = 15000;
constexpr int PONG_CHECK_INTERVAL_MS = 5000;
constexpr int PONG_TIMEOUT_MS = 45000;
constexpr int MAX_RECONNECT_INTERVAL_MS = 60000;
constexpr int INITIAL_RECONNECT_INTERVAL_MS = 3000;
constexpr size_t RECV_BUFFER_SIZE = 2 * 1024 * 1024;

TcpWorker::TcpWorker(QObject *parent)
    : QObject(parent),
      _socket(nullptr),
      _host(""),
      _port(0),
      _pending_connect(std::nullopt),
      _recv_buffer(RingBuffer(RECV_BUFFER_SIZE)),
      _head_parsed(false),
      _message_id(0),
      _message_len(0),
      _heartbeat_timer(nullptr),
      _pong_check_timer(nullptr),
      _reconnect_timer(nullptr),
      _reconnect_interval(INITIAL_RECONNECT_INTERVAL_MS),
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
        ServerInfo si;
        {
            QMutexLocker locker(&_pending_connect_mutex);
            si = *_pending_connect;
            _pending_connect.reset();
        }
        slot_tcp_connect(si);
    }
}

void TcpWorker::slot_stop()
{
    QMutexLocker locker(&_pending_connect_mutex);
    _state.store(ConnectionState::Stopping);
    _pending_connect.reset();

    if (_heartbeat_timer) { _heartbeat_timer->stop(); _heartbeat_timer->disconnect(); }
    if (_pong_check_timer) { _pong_check_timer->stop(); _pong_check_timer->disconnect(); }
    if (_reconnect_timer) { _reconnect_timer->stop(); _reconnect_timer->disconnect(); }

    if (_socket)
    {
        _socket->disconnect();
        _socket->abort();
    }
    reset_buffer();
}

void TcpWorker::slot_tcp_connect(ServerInfo si)
{
    QMutexLocker locker(&_pending_connect_mutex);
    if (!_socket)
    {
        _pending_connect = si;
        return;
    }

    {
        QMutexLocker locker(&_host_port_mutex);
        _host = si.Host;
        _port = static_cast<uint16_t>(si.Port.toUInt());
    }
    _pending_connect = si;
    _reconnect_interval = INITIAL_RECONNECT_INTERVAL_MS;
    _last_pong_time = 0;
    stop_timers();
    reset_buffer();

    _socket->abort();
    _state.store(ConnectionState::Connecting);
    _socket->connectToHost(_host, _port);
}

void TcpWorker::slot_send_data(RequestType reqId, const QByteArray &data)
{
    if (!_socket)
    {
        qWarning() << "Tcp send rejected: socket not initialized, reqId:" << static_cast<int>(reqId);
        return;
    }
    if (!can_send())
    {
        qWarning() << "Tcp send rejected: connection state is not Connected, state:" << static_cast<int>(_state.load()) << "reqId:" << static_cast<int>(reqId);
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
    qDebug() << "Tcp Send: ID=" << id << "(" << static_cast<int>(reqId) << ") Len=" << len;
}

void TcpWorker::slot_connected()
{
    const auto current_state = _state.load();
    const bool was_reconnecting = current_state == ConnectionState::Reconnecting;

    if (_reconnect_timer->isActive())
    {
        _reconnect_timer->stop();
    }
    _reconnect_interval = INITIAL_RECONNECT_INTERVAL_MS;
    _last_pong_time = QDateTime::currentMSecsSinceEpoch();
    _state.store(ConnectionState::Connected);

    _heartbeat_timer->start(HEARTBEAT_INTERVAL_MS);
    _pong_check_timer->start(PONG_CHECK_INTERVAL_MS);

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
        if (!_head_parsed)
        {
            if (_recv_buffer.Available() < 6)
            {
                break;
            }

            char header[6];
            _recv_buffer.Read(header, 6);

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
                _head_parsed = false;
                _message_len = 0;
                _socket->disconnectFromHost();
                return;
            }

            _head_parsed = true;
        }

        if (_head_parsed)
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

            _head_parsed = false;
        }
    }
}

void TcpWorker::slot_error(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)

    if (_state.load() == ConnectionState::Stopping)
    {
        return;
    }

    stop_timers();
    emit sig_con_success(false);
    schedule_reconnect();
}

void TcpWorker::slot_disconnected()
{
    if (_state.load() == ConnectionState::Stopping)
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

    if (_last_pong_time > 0 && elapsed > PONG_TIMEOUT_MS)
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

    QString host;
    uint16_t port;
    {
        QMutexLocker locker(&_host_port_mutex);
        host = _host;
        port = _port;
    }
    if (host.isEmpty() || port == 0)
    {
        qWarning() << "Reconnect skipped: missing target host or port";
        _state.store(ConnectionState::Idle);
        return;
    }

    reset_buffer();
    _state.store(ConnectionState::Connecting);
    if (_socket)
    {
        _socket->connectToHost(host, port);
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
    _recv_buffer.Read(result.data(), static_cast<std::size_t>(len));
    return result;
}

void TcpWorker::schedule_reconnect()
{
    if (!_socket || _state.load() == ConnectionState::Stopping)
    {
        return;
    }

    QString host;
    uint16_t port;
    {
        QMutexLocker locker(&_host_port_mutex);
        host = _host;
        port = _port;
    }
    if (host.isEmpty() || port == 0)
    {
        _state.store(ConnectionState::Idle);
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

    _state.store(ConnectionState::Reconnecting);
    _reconnect_timer->start(_reconnect_interval);
    _reconnect_interval = (std::min)(_reconnect_interval * 2, MAX_RECONNECT_INTERVAL_MS);
}

void TcpWorker::reset_buffer()
{
    _recv_buffer.Clear();
    _head_parsed = false;
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
    return _state.load() == ConnectionState::Connected && _socket && _socket->state() == QAbstractSocket::ConnectedState;
}

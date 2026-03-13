#include "tcpworker.h"
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
      _pending_connect(),
      _has_pending_connect(false),
      _buffered_bytes(0),
      _b_head_parsed(false),
      _message_id(0),
      _message_len(0),
      _heartbeat_timer(nullptr),
      _pong_check_timer(nullptr),
      _reconnect_timer(nullptr),
      _reconnect_interval(3000),
      _is_first_connection(true),
      _last_pong_time(0)
{
}

TcpWorker::~TcpWorker()
{
    slot_stop();
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

    if (_has_pending_connect)
    {
        ServerInfo si = _pending_connect;
        _has_pending_connect = false;
        slot_tcp_connect(si);
    }
}

void TcpWorker::slot_stop()
{
    if (_heartbeat_timer && _heartbeat_timer->isActive())
    {
        _heartbeat_timer->stop();
    }
    if (_pong_check_timer && _pong_check_timer->isActive())
    {
        _pong_check_timer->stop();
    }
    if (_reconnect_timer && _reconnect_timer->isActive())
    {
        _reconnect_timer->stop();
    }
    if (_socket)
    {
        _socket->disconnectFromHost();
        _socket->close();
    }
    reset_buffer();
}

void TcpWorker::slot_tcp_connect(ServerInfo si)
{
    if (!_socket)
    {
        _pending_connect = si;
        _has_pending_connect = true;
        return;
    }

    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _is_first_connection = true;
    _reconnect_interval = 3000;
    _last_pong_time = 0;
    reset_buffer();

    _socket->abort();
    _socket->connectToHost(_host, _port);
}

void TcpWorker::slot_send_data(RequestType reqId, const QString &data)
{
    if (!_socket)
    {
        return;
    }

    uint16_t id = static_cast<uint16_t>(reqId);
    QByteArray dataBytes = data.toUtf8();
    quint32 len = static_cast<quint32>(dataBytes.size());

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << id << len;
    block.append(dataBytes);

    _socket->write(block);
    qDebug() << "Tcp Send: ID=" << id << " Len=" << len << " Data=" << data;
}

void TcpWorker::slot_connected()
{
    if (_reconnect_timer->isActive())
    {
        _reconnect_timer->stop();
    }
    _reconnect_interval = 3000;
    _last_pong_time = QDateTime::currentMSecsSinceEpoch();

    _heartbeat_timer->start(15000);
    _pong_check_timer->start(5000);

    if (!_is_first_connection)
    {
        emit sig_reconnected();
    }
    else
    {
        _is_first_connection = false;
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
        _chunks.push_back(BufferChunk{data, 0});
        _buffered_bytes += data.size();
    }

    while (true)
    {
        if (!_b_head_parsed)
        {
            if (_buffered_bytes < 6)
            {
                break;
            }

            char header[6];
            if (!peekBytes(0, header, 6))
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
                _socket->disconnectFromHost();
                return;
            }

            consumeBytes(6);
            _b_head_parsed = true;
        }

        if (_b_head_parsed)
        {
            if (_buffered_bytes < static_cast<qsizetype>(_message_len))
            {
                break;
            }

            QByteArray messageBody = readBytes(static_cast<qsizetype>(_message_len));
            qDebug() << "Recv Packet: ID=" << _message_id << " Len=" << _message_len;

            if (_message_id == 1000)
            {
                _last_pong_time = QDateTime::currentMSecsSinceEpoch();
                qDebug() << "Pong received, updated _last_pong_time";
            }

            emit sig_msg_received(static_cast<RequestType>(_message_id), messageBody);
            emit sig_msg_received(_message_id, messageBody);

            _b_head_parsed = false;
        }
    }
}

void TcpWorker::slot_error(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)

    if (_heartbeat_timer->isActive())
    {
        _heartbeat_timer->stop();
    }
    if (_pong_check_timer->isActive())
    {
        _pong_check_timer->stop();
    }

    if (_socket->state() != QAbstractSocket::ConnectedState)
    {
        schedule_reconnect();
    }

    emit sig_con_success(false);
}

void TcpWorker::slot_disconnected()
{
    if (_heartbeat_timer->isActive())
    {
        _heartbeat_timer->stop();
    }
    if (_pong_check_timer->isActive())
    {
        _pong_check_timer->stop();
    }

    schedule_reconnect();
}

void TcpWorker::slot_send_ping()
{
    slot_send_data(static_cast<RequestType>(1000), "{}");
}

void TcpWorker::slot_pong_check()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - _last_pong_time;

    if (_last_pong_time > 0 && elapsed > 45000)
    {
        _socket->disconnectFromHost();
    }
}

void TcpWorker::slot_reconnect_timeout()
{
    if (!_socket)
    {
        return;
    }
    _socket->connectToHost(_host, _port);
}

bool TcpWorker::peekBytes(qsizetype offset, char *dest, qsizetype len) const
{
    if (len <= 0 || offset < 0)
    {
        return false;
    }
    if (_buffered_bytes < offset + len)
    {
        return false;
    }

    qsizetype remaining = offset;
    qsizetype copied = 0;

    for (const auto &chunk : _chunks)
    {
        const qsizetype available = chunk.data.size() - chunk.offset;
        if (available <= 0)
        {
            continue;
        }

        if (remaining >= available)
        {
            remaining -= available;
            continue;
        }

        const char *src = chunk.data.constData() + chunk.offset + remaining;
        const qsizetype take = qMin(len - copied, available - remaining);
        memcpy(dest + copied, src, static_cast<size_t>(take));
        copied += take;
        remaining = 0;

        if (copied >= len)
        {
            return true;
        }
    }

    return copied == len;
}

void TcpWorker::consumeBytes(qsizetype len)
{
    if (len <= 0)
    {
        return;
    }

    qsizetype remaining = len;
    while (remaining > 0 && !_chunks.empty())
    {
        BufferChunk &front = _chunks.front();
        const qsizetype available = front.data.size() - front.offset;
        if (available <= 0)
        {
            _chunks.pop_front();
            continue;
        }

        const qsizetype take = qMin(remaining, available);
        front.offset += static_cast<int>(take);
        remaining -= take;
        _buffered_bytes -= take;

        if (front.offset >= front.data.size())
        {
            _chunks.pop_front();
        }
    }
}

QByteArray TcpWorker::readBytes(qsizetype len)
{
    QByteArray out;
    if (len <= 0)
    {
        return out;
    }
    out.resize(static_cast<int>(len));

    qsizetype remaining = len;
    qsizetype pos = 0;
    while (remaining > 0 && !_chunks.empty())
    {
        BufferChunk &front = _chunks.front();
        const qsizetype available = front.data.size() - front.offset;
        if (available <= 0)
        {
            _chunks.pop_front();
            continue;
        }

        const qsizetype take = qMin(remaining, available);
        memcpy(out.data() + pos, front.data.constData() + front.offset, static_cast<size_t>(take));
        front.offset += static_cast<int>(take);
        remaining -= take;
        pos += take;
        _buffered_bytes -= take;

        if (front.offset >= front.data.size())
        {
            _chunks.pop_front();
        }
    }
    return out;
}

void TcpWorker::schedule_reconnect()
{
    if (_reconnect_timer->isActive())
    {
        return;
    }

    _reconnect_timer->start(_reconnect_interval);
    _reconnect_interval *= 2;
    if (_reconnect_interval > 30000)
    {
        _reconnect_interval = 30000;
    }
}

void TcpWorker::reset_buffer()
{
    _chunks.clear();
    _buffered_bytes = 0;
    _b_head_parsed = false;
    _message_id = 0;
    _message_len = 0;
}

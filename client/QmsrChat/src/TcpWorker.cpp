/**
 * @file tcpworker.cpp
 * @brief TCP 工作线程实现
 * @details 负责长连接生命周期、协议解包、心跳检测与自动重连。
 */
#include "TcpWorker.h"
#include "Message.pb.h"
#include "FileRecvMgr.h"
#include <QAbstractSocket>
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtEndian>
#include <cstring>

TcpWorker::TcpWorker(QObject *parent)
    : QObject(parent),
      _socket(nullptr),
      _host(""),
      _port(0),
      _pending_connect(),
      _has_pending_connect(false),
      _recv_buffer(RingBuffer(2 * 1024 * 1024)),
      _b_head_parsed(false),
      _message_id(0),
      _message_len(0),
      _heartbeat_timer(nullptr),
      _pong_check_timer(nullptr),
      _reconnect_timer(nullptr),
      _reconnect_interval(3000),
      _is_first_connection(true),
      _last_pong_time(0),
      _b_bin_head_parsed(false),
      _bin_message_id(0),
      _bin_total_len(0),
      _bin_json_len(0)
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
            else if (_message_id == static_cast<quint16>(RequestType::MSG_FILE_CHUNK))
            {
                if (!_b_bin_head_parsed)
                {
                    if (_recv_buffer.Available() < HEAD_BIN_TOTAL_LEN)
                    {
                        _b_head_parsed = false;
                        break;
                    }

                    char bin_header[HEAD_BIN_TOTAL_LEN];
                    if (!peekBytes(0, bin_header, HEAD_BIN_TOTAL_LEN))
                    {
                        _b_head_parsed = false;
                        break;
                    }

                    _bin_message_id = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(bin_header));
                    _bin_total_len = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bin_header + 2));
                    _bin_json_len = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bin_header + 6));

                    if (_bin_total_len == 0 || _bin_json_len > _bin_total_len || _bin_total_len > HEAD_BIN_MAX_LENGTH)
                    {
                        qWarning() << "SECURITY: Invalid binary packet header. TotalLen=" << _bin_total_len
                                   << " JsonLen=" << _bin_json_len;
                        _socket->abort();
                        return;
                    }

                    consumeBytes(HEAD_BIN_TOTAL_LEN);
                    _b_bin_head_parsed = true;
                    continue;
                }

                if (_recv_buffer.Available() < static_cast<std::size_t>(_bin_total_len))
                {
                    break;
                }

                QByteArray bin_body = readBytes(static_cast<qsizetype>(_bin_total_len));

                QString json_str = QString::fromUtf8(bin_body.left(_bin_json_len));
                QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8());

                if (!doc.isNull() && doc.isObject())
                {
                    QJsonObject header = doc.object();
                    int64_t task_id = header["task_id"].toInteger();
                    int64_t offset = header["offset"].toInteger();
                    int64_t chunk_size = header["size"].toInteger();

                    QByteArray chunk_data = bin_body.mid(_bin_json_len);

                    if (!chunk_data.isEmpty())
                    {
                        FileRecvMgr::Instance().WriteChunk(task_id, chunk_data.constData(), chunk_data.size());
                    }

                    QJsonObject ack;
                    ack["task_id"] = task_id;
                    ack["received"] = offset + chunk_data.size();
                    QJsonDocument ack_doc(ack);
                    slot_send_data(RequestType::MSG_FILE_ACK, ack_doc.toJson(QJsonDocument::Compact));
                }

                _b_head_parsed = false;
                _b_bin_head_parsed = false;
                continue;
            }
            else if (_message_id == static_cast<quint16>(RequestType::MSG_FILE_RSP))
            {
                qmsrchat::FileRsp fileRsp;
                if (fileRsp.ParseFromArray(messageBody.constData(), messageBody.size()))
                {
                    int64_t task_id = fileRsp.task_id();
                    int error = fileRsp.error();
                    int64_t offset = fileRsp.offset();

                    if (error == 0 && offset > 0)
                    {
                        qmsrchat::FileRsp ack;
                        ack.set_task_id(task_id);
                        ack.set_offset(offset);

                        std::string serialized;
                        if (ack.SerializeToString(&serialized))
                        {
                            slot_send_data(RequestType::MSG_FILE_RSP, QString::fromStdString(serialized));
                        }
                    }
                }
                _b_head_parsed = false;
                continue;
            }
            else if (_message_id == static_cast<quint16>(RequestType::MSG_FILE_ACK))
            {
                qmsrchat::FileAck fileAck;
                if (fileAck.ParseFromArray(messageBody.constData(), messageBody.size()))
                {
                    int64_t task_id = fileAck.task_id();
                    int64_t received = fileAck.received();
                    FileRecvMgr::Instance().OnChunkAck(task_id, received);
                }
                _b_head_parsed = false;
                continue;
            }
            else
            {
                qDebug() << "Recv message ID=" << _message_id << " forwarded to TcpMgr for parsing";
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

    if (_heartbeat_timer && _heartbeat_timer->isActive())
    {
        _heartbeat_timer->stop();
    }
    if (_pong_check_timer && _pong_check_timer->isActive())
    {
        _pong_check_timer->stop();
    }

    if (_socket && _socket->state() != QAbstractSocket::ConnectedState)
    {
        schedule_reconnect();
    }

    emit sig_con_success(false);
}

void TcpWorker::slot_disconnected()
{
    if (_heartbeat_timer && _heartbeat_timer->isActive())
    {
        _heartbeat_timer->stop();
    }
    if (_pong_check_timer && _pong_check_timer->isActive())
    {
        _pong_check_timer->stop();
    }

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
    if (_socket)
    {
        _socket->connectToHost(_host, _port);
    }
}

bool TcpWorker::peekBytes(qsizetype offset, char *dest, qsizetype len) const
{
    if (len <= 0 || offset < 0)
    {
        return false;
    }
    return _recv_buffer.Peek(static_cast<std::size_t>(offset), dest, static_cast<std::size_t>(len));
}

void TcpWorker::consumeBytes(qsizetype len)
{
    if (len > 0)
    {
        _recv_buffer.Consume(static_cast<std::size_t>(len));
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
    if (_socket && _socket->state() == QAbstractSocket::UnconnectedState)
    {
        _reconnect_timer->start(_reconnect_interval);
        _reconnect_interval = (std::min)(_reconnect_interval * 2, 60000);
    }
}

void TcpWorker::reset_buffer()
{
    _recv_buffer.Clear();
    _b_head_parsed = false;
    _b_bin_head_parsed = false;
}

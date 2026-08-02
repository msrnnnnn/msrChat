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
constexpr int HEARTBEAT_INTERVAL_MS = 30000;
constexpr int PONG_CHECK_INTERVAL_MS = 5000;
constexpr int PONG_TIMEOUT_MS = 45000;
constexpr int MAX_RECONNECT_INTERVAL_MS = 60000;
constexpr int INITIAL_RECONNECT_INTERVAL_MS = 3000;
constexpr size_t RECV_BUFFER_SIZE = 2 * 1024 * 1024;

TcpWorker::TcpWorker(QObject *parent)
    : QObject(parent), _socket(nullptr), _host(""), _port(0), _pending_connect(std::nullopt),
      _recv_buffer(RingBuffer(RECV_BUFFER_SIZE)), _head_parsed(false), _message_id(0), _message_len(0),
      _heartbeat_timer(nullptr), _pong_check_timer(nullptr), _reconnect_timer(nullptr),
      _reconnect_interval(INITIAL_RECONNECT_INTERVAL_MS), _last_pong_time(0), _state(ConnectionState::Idle)
{
}

TcpWorker::~TcpWorker()
{
    // 防御性清理：若 TcpMgr 未能通过 BlockingQueuedConnection 调用 slot_stop()
    // （如异常关机路径），此处兜底停止 timer 和 socket
    slot_stop();
}

/**
 * @brief 在工作线程中初始化 socket 和定时器
 * @details 创建 QTcpSocket、心跳/重连定时器，绑定信号槽。
 *          若有延迟连接请求（_pending_connect），在此处补发。
 */
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
        // 延迟连接：slot_init 之前收到的连接请求，在此补发
        ServerInfo si;
        {
            QMutexLocker locker(&_pending_connect_mutex);
            si = *_pending_connect;
            _pending_connect.reset();
        }
        slotTcpConnect(si);
    }
}

/**
 * @brief 停止工作线程，清理所有资源
 * @details 设置 Stopping 状态以防止重连触发，停止并断开所有定时器，
 *          断开并 abort socket，最后重置接收缓冲区。
 */
void TcpWorker::slot_stop()
{
    QMutexLocker locker(&_pending_connect_mutex);
    _state.store(ConnectionState::Stopping);
    _pending_connect.reset();

    if (_heartbeat_timer)
    {
        _heartbeat_timer->stop();
        _heartbeat_timer->disconnect();
    }
    if (_pong_check_timer)
    {
        _pong_check_timer->stop();
        _pong_check_timer->disconnect();
    }
    if (_reconnect_timer)
    {
        _reconnect_timer->stop();
        _reconnect_timer->disconnect();
    }

    if (_socket)
    {
        _socket->disconnect();
        _socket->abort();
    }
    reset_buffer();
}

/**
 * @brief 发起 TCP 连接
 * @param si 服务器连接信息（主机、端口）
 * @details 若 socket 尚未初始化（slot_init 未调用），将连接参数缓存到 _pending_connect，
 *          等待 slot_init 时补发。已初始化则直接连接。
 */
void TcpWorker::slotTcpConnect(ServerInfo si)
{
    QMutexLocker locker(&_pending_connect_mutex);
    if (!_socket)
    {
        _pending_connect = si;
        return;
    }

    QString host;
    uint16_t port;
    {
        QMutexLocker locker(&_host_port_mutex);
        _host = si.Host;
        _port = static_cast<uint16_t>(si.Port.toUInt());
        host = _host;
        port = _port;
    }

    qDebug() << "[TcpWorker] Connecting to" << host << ":" << port;

    _reconnect_interval = INITIAL_RECONNECT_INTERVAL_MS;
    _last_pong_time = 0;
    stop_timers();
    reset_buffer();

    _socket->abort();
    _state.store(ConnectionState::Connecting);
    _socket->connectToHost(host, port);
}

/**
 * @brief 发送数据到 socket
 * @param reqId 请求类型 ID
 * @param data 负载数据
 * @details 将数据打包为 [2字节ID|4字节长度|负载] 的大端格式后写入 socket。
 *          发送前检查 socket 是否存在且连接状态正常。
 */
void TcpWorker::slotSendData(RequestType reqId, const QByteArray &data)
{
    if (!_socket)
    {
        qWarning() << "Tcp send rejected: socket not initialized, reqId:" << static_cast<int>(reqId);
        return;
    }
    if (!can_send())
    {
        qWarning() << "Tcp send rejected: connection state is not Connected, state:" << static_cast<int>(_state.load())
                   << "reqId:" << static_cast<int>(reqId);
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

/**
 * @brief socket 连接成功回调
 * @details 更新连接状态为 Connected，重置重连退避间隔和心跳时间戳，
 *          启动心跳和 Pong 检测定时器。若之前处于重连状态则发出重连成功信号。
 */
void TcpWorker::slot_connected()
{
    qDebug() << "[TcpWorker] slot_connected fired! state before:" << static_cast<int>(_state.load());
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
        emit sigReconnected();
    }

    emit sigConSuccess(true);
}

/**
 * @brief socket 可读回调，解析 TCP 粘包/拆包
 * @details 协议格式：[2字节大端ID|4字节大端长度|负载]。
 *          使用环形缓冲区缓存数据，循环解析直到数据不足时退出。
 *          包含安全校验：数据量超限、消息长度超限、空消息均会断开连接。
 *          心跳回应（MSG_HELLO）在此层拦截，刷新 _last_pong_time。
 */
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
                emit sigPacketReceived(_message_id, messageBody);
            }

            _head_parsed = false;
        }
    }
}

/**
 * @brief socket 错误回调
 * @param error 错误类型（未使用）
 * @details Stopping 状态下忽略错误，否则停止定时器、通知上层断开、触发重连。
 */
void TcpWorker::slot_error(QAbstractSocket::SocketError error)
{
    qWarning() << "[TcpWorker] slot_error:" << error << _socket->errorString() << "state:" << static_cast<int>(_state.load());

    if (_state.load() == ConnectionState::Stopping)
    {
        return;
    }

    stop_timers();
    emit sigConSuccess(false);
    schedule_reconnect();
}

/**
 * @brief socket 断开连接回调
 * @details 与 slot_error 处理逻辑一致：Stopping 状态忽略，否则触发重连流程。
 */
void TcpWorker::slot_disconnected()
{
    qWarning() << "[TcpWorker] slot_disconnected, state:" << static_cast<int>(_state.load());
    if (_state.load() == ConnectionState::Stopping)
    {
        return;
    }

    stop_timers();
    emit sigConSuccess(false);
    schedule_reconnect();
}

/**
 * @brief 发送心跳 Ping 包（MSG_HELLO + "{}"）
 * @note "{}" 是 2 字节 JSON 对象，QByteArray 构造时不会额外添加 null 终止符
 *       （QByteArray(const char*) 依赖 strlen 确定长度，"{}" 长度为 2）
 */
void TcpWorker::slot_send_ping()
{
    slotSendData(RequestType::MSG_HELLO, "{}");
}

/**
 * @brief 检测 Pong 超时
 * @details 若距离最近一次 Pong 响应超过 PONG_TIMEOUT_MS（45s），
 *          认为连接已断，主动断开 socket 以触发重连。
 */
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

/**
 * @brief 重连定时器到期，尝试重新连接
 * @details 仅在 Reconnecting 状态下有效。重置缓冲区后发起 connectToHost。
 */
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

/**
 * @brief 从环形缓冲区读取指定长度的数据
 * @param len 读取长度
 * @return QByteArray 读取到的数据
 */
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

/**
 * @brief 调度自动重连（指数退避）
 * @details 重连间隔从 INITIAL_RECONNECT_INTERVAL_MS（3s）开始，每次翻倍，
 *          最大不超过 MAX_RECONNECT_INTERVAL_MS（60s）。
 *          连接成功后由 slot_connected 重置退避间隔。
 */
void TcpWorker::schedule_reconnect()
{
    // Stopping 状态下不重连
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
    // 指数退避：每次翻倍，上限 60s
    _reconnect_interval = (std::min)(_reconnect_interval * 2, MAX_RECONNECT_INTERVAL_MS);
}

/**
 * @brief 重置接收缓冲区和解析状态
 */
void TcpWorker::reset_buffer()
{
    _recv_buffer.Clear();
    _head_parsed = false;
    _message_id = 0;
    _message_len = 0;
}

/**
 * @brief 停止所有定时器（心跳、Pong检测、重连）
 */
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

/**
 * @brief 检查是否允许发送数据
 * @return true 可发送, false 不可发送
 * @note 同时检查连接状态与 socket 底层状态，防止在未完全建立连接时发送数据。
 */
bool TcpWorker::can_send() const
{
    return _state.load() == ConnectionState::Connected && _socket &&
           _socket->state() == QAbstractSocket::ConnectedState;
}

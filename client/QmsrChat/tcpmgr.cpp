#include "tcpmgr.h"
#include <QAbstractSocket>
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

TcpMgr::TcpMgr()
    : _host(""),
      _port(0),
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
    // 初始化心跳定时器 (发送 Ping)
    _heartbeat_timer = new QTimer(this);
    connect(
        _heartbeat_timer, &QTimer::timeout,
        [this]()
        {
            // 发送心跳包 (ID=1000, 空JSON)
            slot_send_data(static_cast<RequestType>(1000), "{}");
            qDebug() << "Heartbeat sent (Ping)";
        });

    // 初始化 Pong 超时检测定时器 (5秒检查一次)
    _pong_check_timer = new QTimer(this);
    connect(
        _pong_check_timer, &QTimer::timeout,
        [this]()
        {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 elapsed = now - _last_pong_time;
            qDebug() << "Pong check: elapsed" << elapsed << "ms, last_pong_time:" << _last_pong_time;

            // 如果超过 45 秒没收到 Pong，视为掉线
            if (_last_pong_time > 0 && elapsed > 45000)
            {
                qDebug() << "Pong timeout (" << elapsed << "ms), disconnecting...";
                _socket.disconnectFromHost();
            }
        });

    // 初始化重连定时器
    _reconnect_timer = new QTimer(this);
    connect(
        _reconnect_timer, &QTimer::timeout,
        [this]()
        {
            qDebug() << "Reconnecting to server... Interval:" << _reconnect_interval << "ms";
            _socket.connectToHost(_host, _port);
        });

    // 连接建立信号
    connect(
        &_socket, &QTcpSocket::connected,
        [this]()
        {
            qDebug() << "Connected to server!";

            // 停止重连定时器
            if (_reconnect_timer->isActive())
            {
                _reconnect_timer->stop();
            }

            // 恢复初始重连间隔
            _reconnect_interval = 3000;

            // 重置 Pong 时间戳
            _last_pong_time = QDateTime::currentMSecsSinceEpoch();

            // 启动心跳定时器 (15秒间隔)
            _heartbeat_timer->start(15000);
            qDebug() << "Heartbeat timer started (15s interval)";

            // 启动 Pong 超时检测定时器 (5秒检查一次)
            _pong_check_timer->start(5000);
            qDebug() << "Pong check timer started (5s interval)";

            // 如果不是首次连接，说明是断线重连，发出重连信号
            if (!_is_first_connection)
            {
                qDebug() << "Reconnection detected, emitting sig_reconnected";
                emit sig_reconnected();
            }
            else
            {
                _is_first_connection = false;
            }

            // 发送连接成功信号，通知业务层（如 LoginDialog）
            emit sig_con_success(true);
        });

    // 接收数据核心逻辑（处理 TCP 粘包）
    connect(
        &_socket, &QTcpSocket::readyRead,
        [this]()
        {
            _buffer.append(_socket.readAll());

            while (true)
            {
                // 解析头部
                if (!_b_head_parsed)
                {
                    // 包头长度为 4 字节 (ID:2 + Len:2)
                    if (_buffer.size() < 4)
                    {
                        return;
                    }
                    const char *data = _buffer.constData();
                    _message_id = qFromBigEndian<quint16>(data);
                    _message_len = qFromBigEndian<quint16>(data + 2);

                    // ========== 安全检查：包长拦截 ==========
                    if (_message_len > MAX_MESSAGE_LEN)
                    {
                        qWarning() << "SECURITY: Malicious packet detected! Message length" << _message_len
                                   << "exceeds limit" << MAX_MESSAGE_LEN;
                        _socket.abort();
                        return;
                    }

                    // 移除已解析的头部 (性能优化：使用 remove 替代 mid)
                    _buffer.remove(0, 4);
                    _b_head_parsed = true;

                    // 新增：处理消息长度为0的情况
                    if (_message_len == 0)
                    {
                        qWarning() << "WARNING: Empty message received. Closing connection.";
                        _socket.disconnectFromHost();
                        return;
                    }
                }

                // 解析包体
                if (_b_head_parsed)
                {
                    // 检查缓冲区是否包含完整的包体
                    if (_buffer.size() < _message_len)
                    {
                        return;
                    }
                    QByteArray messageBody = _buffer.mid(0, _message_len);
                    qDebug() << "Recv Packet: ID=" << _message_id << " Len=" << _message_len;

                    // ========== 双向心跳：收到 Pong 回包时更新时间戳 ==========
                    if (_message_id == 1000)
                    {
                        _last_pong_time = QDateTime::currentMSecsSinceEpoch();
                        qDebug() << "Pong received, updated _last_pong_time";
                    }

                    // 通知业务层处理完整消息
                    emit sig_msg_received(static_cast<RequestType>(_message_id), messageBody);
                    emit sig_msg_received(_message_id, messageBody);

                    // 移除已解析的包体 (性能优化：使用 remove 替代 mid)
                    _buffer.remove(0, _message_len);
                    _b_head_parsed = false;
                }
            }
        });

    // 错误处理 - 触发重连
    connect(
        &_socket, &QTcpSocket::errorOccurred,
        [this](QAbstractSocket::SocketError error)
        {
            qDebug() << "Socket Error:" << _socket.errorString() << "Error code:" << error;

            // 停止心跳定时器
            if (_heartbeat_timer->isActive())
            {
                _heartbeat_timer->stop();
                qDebug() << "Heartbeat timer stopped due to error";
            }

            // 停止 Pong 检测定时器
            if (_pong_check_timer->isActive())
            {
                _pong_check_timer->stop();
                qDebug() << "Pong check timer stopped due to error";
            }

            // 如果 socket 未连接，启动重连定时器
            if (_socket.state() != QAbstractSocket::ConnectedState)
            {
                qDebug() << "Network disconnected, starting reconnect timer. Interval:" << _reconnect_interval << "ms";
                _reconnect_timer->start(_reconnect_interval);

                // 指数退避：下次重连间隔翻倍，最大30秒
                _reconnect_interval *= 2;
                if (_reconnect_interval > 30000)
                {
                    _reconnect_interval = 30000;
                }
            }

            emit sig_con_success(false);
        });

    // 断开连接处理 - 触发重连
    connect(
        &_socket, &QTcpSocket::disconnected,
        [this]()
        {
            qDebug() << "Socket disconnected from server";

            // 停止心跳定时器
            if (_heartbeat_timer->isActive())
            {
                _heartbeat_timer->stop();
                qDebug() << "Heartbeat timer stopped due to disconnect";
            }

            // 停止 Pong 检测定时器
            if (_pong_check_timer->isActive())
            {
                _pong_check_timer->stop();
                qDebug() << "Pong check timer stopped due to disconnect";
            }

            // 启动重连定时器
            qDebug() << "Starting reconnect timer. Interval:" << _reconnect_interval << "ms";
            _reconnect_timer->start(_reconnect_interval);

            // 指数退避：下次重连间隔翻倍，最大30秒
            _reconnect_interval *= 2;
            if (_reconnect_interval > 30000)
            {
                _reconnect_interval = 30000;
            }
        });
}

TcpMgr::~TcpMgr()
{
    // 停止定时器
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
    _socket.close();
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug() << "receive tcp connect signal";
    qDebug() << "Connecting to server...";

    _host = "192.168.226.129";
    _port = static_cast<uint16_t>(si.Port.toUInt());

    // 重置首次连接标记
    _is_first_connection = true;
    _reconnect_interval = 3000;
    _last_pong_time = 0;

    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(RequestType reqId, const QString& data)
{
    uint16_t id = static_cast<uint16_t>(reqId);

    // 1. 将字符串转换为UTF-8编码的字节数组
    QByteArray dataBytes = data.toUtf8();

    // 2. 计算包体长度
    quint16 len = static_cast<quint16>(dataBytes.size());

    // 3. 构建发送缓冲区
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    // 4. 设置网络字节序 (BigEndian)
    out.setByteOrder(QDataStream::BigEndian);

    // 5. 写入头部：ID(2字节) + 长度(2字节)
    out << id << len;

    // 6. 写入包体数据
    block.append(dataBytes);

    // 7. 发送数据
    _socket.write(block);

    qDebug() << "Tcp Send: ID=" << id << " Len=" << len << " Data=" << data;
}

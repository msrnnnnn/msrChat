#include "tcpmgr.h"
#include <QDataStream>
#include <QDebug>
#include <QAbstractSocket>

TcpMgr::TcpMgr() : _host(""), _port(0), _b_head_parsed(false), _message_id(0), _message_len(0)
{
    // 连接建立信号
    connect(&_socket, &QTcpSocket::connected, [this]() {
        qDebug() << "Connected to server!";
        // 发送连接成功信号，通知业务层（如 LoginDialog）
        emit sig_con_success(true);
    });

    // 接收数据核心逻辑（处理 TCP 粘包）
    connect(&_socket, &QTcpSocket::readyRead, [this]() {
        _buffer.append(_socket.readAll());

        while (true) {
            // 解析头部
            if (!_b_head_parsed) {
                // 包头长度为 4 字节 (ID:2 + Len:2)
                if (_buffer.size() < 4) {
                    return;
                }
                QDataStream stream(_buffer);
                stream.setByteOrder(QDataStream::BigEndian);
                stream >> _message_id >> _message_len;

                // 移除已解析的头部
                _buffer = _buffer.mid(4);
                _b_head_parsed = true;
            }

            // 解析包体
            if (_b_head_parsed) {
                // 检查缓冲区是否包含完整的包体
                if (_buffer.size() < _message_len) {
                    return;
                }
                QByteArray messageBody = _buffer.mid(0, _message_len);
                qDebug() << "Recv Packet: ID=" << _message_id << " Len=" << _message_len;

                // 通知业务层处理完整消息
                emit sig_msg_received(static_cast<RequestType>(_message_id), messageBody);

                // 移除已解析的包体，重置状态以解析下一个包
                _buffer = _buffer.mid(_message_len);
                _b_head_parsed = false;
            }
        }
    });

    // 错误处理
    connect(&_socket, &QTcpSocket::errorOccurred, [this](QAbstractSocket::SocketError) {
        qDebug() << "Socket Error:" << _socket.errorString();
        emit sig_con_success(false);
    });
}

TcpMgr::~TcpMgr() {
    _socket.close();
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug()<< "receive tcp connect signal";
    qDebug() << "Connecting to server...";

    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());

    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(RequestType reqId, QString data)
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

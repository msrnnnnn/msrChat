#include "TcpMgr.h"
#include <QDataStream>
#include <QDebug>

TcpMgr::TcpMgr()
    : _host(""), _port(0),
      _b_head_parsed(false),
      _message_id(0),
      _message_len(0)
{
    // 连接建立信号
    connect(&_socket, &QTcpSocket::connected, [this]() {
        qDebug() << "Connected to server!";
        emit sig_con_success(true);
    });

           // 🌟 核心修复：接收数据处理 (变量名已替换) 🌟
    connect(&_socket, &QTcpSocket::readyRead, [this]() {
        // 1. 读取所有新数据追加到缓冲区
        _buffer.append(_socket.readAll());

        while (true) {
            // --- 阶段 A：解析头部 (4字节) ---
            // 如果头部还没解析 (false)，那就尝试解析头部
            if (!_b_head_parsed) {
                // 如果数据连头部都不够，直接返回，等下次
                if (_buffer.size() < 4) {
                    return;
                }

                       // 使用 QDataStream 读取头部
                QDataStream stream(_buffer);
                stream.setByteOrder(QDataStream::BigEndian); // 网络字节序：大端

                stream >> _message_id >> _message_len;

                       // 头部解析成功，从 buffer 中移除这 4 个字节
                _buffer = _buffer.mid(4);

                       // 🌟 状态流转：头部解析完成，标记为 true，进入等待 Body 阶段
                _b_head_parsed = true;
            }

                   // --- 阶段 B：解析包体 ---
                   // 如果头部已经解析了 (true)，那就尝试解析包体
            if (_b_head_parsed) {
                // 检查缓冲区剩下的数据是否够一个完整的 Body
                if (_buffer.size() < _message_len) {
                    // 不够，说明半包了，返回等待下一次 readyRead
                    return;
                }

                       // 够了！提取 Body
                QByteArray messageBody = _buffer.mid(0, _message_len);

                       // 打印调试
                qDebug() << "Recv Packet: ID=" << _message_id << " Len=" << _message_len;

                       // 发送信号给逻辑层 (类型强转)
                emit sig_msg_received(static_cast<RequestType>(_message_id), messageBody);

                       // 移除已处理的 Body
                _buffer = _buffer.mid(_message_len);

                       // 🌟 状态流转：Body 处理完了，标记为 false，准备处理下一个包的 Head
                _b_head_parsed = false;
            }
        }
    });

           // 错误处理 (适配 Qt 6)
    connect(&_socket, &QTcpSocket::errorOccurred, [this](QAbstractSocket::SocketError socketError) {
        qDebug() << "Socket Error:" << _socket.errorString();
        emit sig_con_success(false);
    });
}

TcpMgr::~TcpMgr() {
    _socket.close();
}

void TcpMgr::slot_tcp_connect(const QString& ip, quint16 port) {
    if (_socket.state() == QAbstractSocket::ConnectedState) return;
    _host = ip;
    _port = port;
    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(RequestType reqId, QString data) {
    if (_socket.state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket Not Connected!";
        return;
    }

           // 构造 TLV 包
    uint16_t id = static_cast<uint16_t>(reqId);
    QByteArray body = data.toUtf8();
    uint16_t len = static_cast<uint16_t>(body.size());

    QByteArray sendBuffer;
    QDataStream out(&sendBuffer, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian); // 关键：大端序

    out << id << len;
    out.writeRawData(body.data(), len);

    _socket.write(sendBuffer);
}

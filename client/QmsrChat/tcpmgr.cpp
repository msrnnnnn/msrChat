#include "TcpMgr.h"
#include <QDataStream>
#include <QDebug>
#include <QAbstractSocket>

TcpMgr::TcpMgr() : _host(""), _port(0), _b_head_parsed(false), _message_id(0), _message_len(0)
{
    // 1. 连接建立信号
    connect(&_socket, &QTcpSocket::connected, [this]() {
        qDebug() << "Connected to server!";
        // 发送连接成功信号，LoginDialog 会收到这个信号然后发起登录
        emit sig_con_success(true);
    });

    // 2. 接收数据核心逻辑 (保持不变，因为这是处理粘包的标准做法)
    connect(&_socket, &QTcpSocket::readyRead, [this]() {
        _buffer.append(_socket.readAll());

        while (true) {
            // --- 阶段 A：解析头部 ---
            if (!_b_head_parsed) {
                if (_buffer.size() < 4) {
                    return;
                }
                QDataStream stream(_buffer);
                stream.setByteOrder(QDataStream::BigEndian);
                stream >> _message_id >> _message_len;

                _buffer = _buffer.mid(4);
                _b_head_parsed = true;
            }

                   // --- 阶段 B：解析包体 ---
            if (_b_head_parsed) {
                if (_buffer.size() < _message_len) {
                    return;
                }
                QByteArray messageBody = _buffer.mid(0, _message_len);
                qDebug() << "Recv Packet: ID=" << _message_id << " Len=" << _message_len;

                       // 通知业务层
                emit sig_msg_received(static_cast<RequestType>(_message_id), messageBody);

                _buffer = _buffer.mid(_message_len);
                _b_head_parsed = false;
            }
        }
    });

           // 3. 错误处理
    connect(&_socket, &QTcpSocket::errorOccurred, [this](QAbstractSocket::SocketError) {
        qDebug() << "Socket Error:" << _socket.errorString();
        emit sig_con_success(false);
    });

    connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
}

TcpMgr::~TcpMgr() {
    _socket.close();
}

// 🌟 按照教程修改：参数使用 ServerInfo 结构体
void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug()<< "receive tcp connect signal";
    qDebug() << "Connecting to server...";

    _host = si.Host;
    // 教程里用的 toUInt 转 int 再强转 uint16
    _port = static_cast<uint16_t>(si.Port.toUInt());

    _socket.connectToHost(_host, _port);
}

// 🌟 按照教程修改：使用 append 拼接数据
void TcpMgr::slot_send_data(RequestType reqId, QString data)
{
    uint16_t id = static_cast<uint16_t>(reqId);

           // 1. 将字符串转换为UTF-8编码的字节数组
           // 比如 "Hello" -> 5个字节
    QByteArray dataBytes = data.toUtf8();

           // 2. 计算长度
           // 注意：这里教程直接取 size，最大支持 65535 字节
    quint16 len = static_cast<quint16>(dataBytes.size());

           // 3. 创建一个QByteArray用于存储要发送的所有数据
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

           // 4. 设置数据流使用网络字节序 (大端)
    out.setByteOrder(QDataStream::BigEndian);

           // 5. 写入头部：ID(2字节) + 长度(2字节)
    out << id << len;

           // 6. 添加字符串数据 (包体)
           // 教程做法：直接把 dataBytes 追加到 block 后面
    block.append(dataBytes);

           // 7. 发送数据
    _socket.write(block);

    // Debug一下看发了啥
    qDebug() << "Tcp Send: ID=" << id << " Len=" << len << " Data=" << data;
}

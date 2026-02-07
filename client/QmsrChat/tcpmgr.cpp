/**
 * @file tcpmgr.cpp
 * @brief TCP 连接管理类实现
 * @author msr
 */

#include "tcpmgr.h"
#include "usermgr.h"
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

TcpMgr::TcpMgr()
    : _host(""),
      _port(0),
      _b_recv_pending(false),
      _message_id(0),
      _message_len(0)
{
    // 注册消息处理器
    initHandlers();

    // 连接成功
    QObject::connect(
        &_socket, &QTcpSocket::connected,
        [this]()
        {
            qDebug() << "Connected to server!";
            // 连接建立后发送信号通知业务层
            emit sig_con_success(true);
        });

    // 接收数据 (处理粘包/拆包)
    QObject::connect(
        &_socket, &QTcpSocket::readyRead,
        [this]()
        {
            // 读取所有数据并追加到缓冲区
            _buffer.append(_socket.readAll());

            QDataStream stream(&_buffer, QIODevice::ReadOnly);
            stream.setVersion(QDataStream::Qt_5_0);

            forever
            {
                // 先解析头部
                if (!_b_recv_pending)
                {
                    // 检查缓冲区中的数据是否足够解析出一个消息头（消息ID + 消息长度 = 4字节）
                    if (_buffer.size() < static_cast<int>(sizeof(quint16) * 2))
                    {
                        return; // 数据不够，等待更多数据
                    }

                    // 读取消息ID和消息长度
                    stream >> _message_id >> _message_len;

                    // 将buffer 中的前四个字节移除
                    _buffer = _buffer.mid(sizeof(quint16) * 2);

                    // 输出读取的数据
                    qDebug() << "Message ID:" << _message_id << ", Length:" << _message_len;
                }

                // buffer剩余长度是否满足消息体长度，不满足则退出继续等待接受
                if (_buffer.size() < _message_len)
                {
                    _b_recv_pending = true;
                    return;
                }

                _b_recv_pending = false;

                // 读取消息体
                QByteArray messageBody = _buffer.mid(0, _message_len);
                qDebug() << "receive body msg is " << messageBody;

                // 移除已处理的消息体
                _buffer = _buffer.mid(_message_len);

                // 处理消息
                handleMsg(static_cast<ReqId>(_message_id), _message_len, messageBody);
            }
        });

    // 处理错误 (Qt 5.15+ 使用 errorOccurred)
    QObject::connect(
        &_socket, &QTcpSocket::errorOccurred,
        [this](QAbstractSocket::SocketError socketError)
        {
            qDebug() << "Error:" << _socket.errorString();
            switch (socketError)
            {
                case QTcpSocket::ConnectionRefusedError:
                    qDebug() << "Connection Refused!";
                    emit sig_con_success(false);
                    break;
                case QTcpSocket::RemoteHostClosedError:
                    qDebug() << "Remote Host Closed Connection!";
                    break;
                case QTcpSocket::HostNotFoundError:
                    qDebug() << "Host Not Found!";
                    emit sig_con_success(false);
                    break;
                case QTcpSocket::SocketTimeoutError:
                    qDebug() << "Connection Timeout!";
                    emit sig_con_success(false);
                    break;
                case QTcpSocket::NetworkError:
                    qDebug() << "Network Error!";
                    break;
                default:
                    qDebug() << "Other Error!";
                    break;
            }
        });

    // 处理连接断开
    QObject::connect(
        &_socket, &QTcpSocket::disconnected,
        [this]()
        {
            qDebug() << "Disconnected from server.";
            // 这里可以考虑添加重连逻辑
        });

    // 连接发送信号 (主要是为了线程安全，如果是多线程调用)
    QObject::connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
}

TcpMgr::~TcpMgr()
{
    // 关闭 socket
    if (_socket.isOpen())
    {
        _socket.close();
    }
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug() << "Connecting to server: " << si.Host << ":" << si.Port;
    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(ReqId reqId, QString data)
{
    uint16_t id = static_cast<uint16_t>(reqId);
    QByteArray dataBytes = data.toUtf8();
    uint16_t len = static_cast<uint16_t>(dataBytes.length());

    QByteArray sendData;
    QDataStream out(&sendData, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_0);
    out.setByteOrder(QDataStream::BigEndian);

    // 写入头部 (ID + Length)
    out << id << len;

    // 写入消息体
    sendData.append(dataBytes);

    _socket.write(sendData);
}

void TcpMgr::initHandlers()
{
    _handlers.insert(
        ReqId::ID_CHAT_LOGIN_RSP,
        [this](ReqId id, int len, QByteArray data)
        {
            Q_UNUSED(len);
            qDebug() << "handle id is " << (int)id << " data is " << data;
            // 将QByteArray转换为QJsonDocument
            QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

            // 检查转换是否成功
            if (jsonDoc.isNull())
            {
                qDebug() << "Failed to create QJsonDocument.";
                return;
            }

            QJsonObject jsonObj = jsonDoc.object();

            if (!jsonObj.contains("error"))
            {
                int err = static_cast<int>(ErrorCodes::ERROR_JSON);
                qDebug() << "Login Failed, err is Json Parse Err" << err;
                emit sig_login_failed(err);
                return;
            }

            int err = jsonObj["error"].toInt();
            if (err != static_cast<int>(ErrorCodes::SUCCESS))
            {
                qDebug() << "Login Failed, err is " << err;
                emit sig_login_failed(err);
                return;
            }

            UserMgr::GetInstance()->SetUid(jsonObj["uid"].toInt());
            UserMgr::GetInstance()->SetName(jsonObj["name"].toString());
            UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());
            emit sig_swich_chatdlg();
        });
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
    auto find_iter = _handlers.find(id);
    if (find_iter == _handlers.end())
    {
        qDebug() << "not found id [" << (int)id << "] to handle";
        return;
    }

    find_iter.value()(id, len, data);
}

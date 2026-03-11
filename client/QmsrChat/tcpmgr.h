/**
 * @file tcpmgr.h
 * @brief TCP 长连接管理类
 * @details 负责与 ChatServer 建立 TCP 连接，处理数据的收发与协议解析。
 *          包含工业级心跳保活与指数退避重连机制。
 */
#ifndef TCPMGR_H
#define TCPMGR_H

#include "global.h"
#include "singleton.h"
#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

/**
 * @class TcpMgr
 * @brief TCP 网络管理单例类
 * @details 管理 QTcpSocket 对象，处理 socket 状态及数据收发。
 *          支持心跳保活和自动重连功能。
 */
class TcpMgr : public QObject, public Singleton<TcpMgr>
{
    Q_OBJECT
    friend class Singleton<TcpMgr>;

public:
    ~TcpMgr();

private:
    /**
     * @brief 私有构造函数
     * @details 初始化 socket 并连接相关信号槽。
     */
    TcpMgr();

    QTcpSocket _socket; ///< TCP 套接字对象
    QString _host;      ///< 服务器主机地址
    uint16_t _port;     ///< 服务器端口号

    QByteArray _buffer;   ///< 接收缓冲区，用于缓存未处理完的数据
    bool _b_head_parsed;  ///< 标志位：当前包头是否已解析
    quint16 _message_id;  ///< 当前消息 ID
    quint32 _message_len; ///< 当前消息体长度

    // 心跳与重连相关成员
    QTimer *_heartbeat_timer;  ///< 心跳定时器 (发送 Ping)
    QTimer *_pong_check_timer; ///< Pong 超时检测定时器
    QTimer *_reconnect_timer;  ///< 重连定时器
    int _reconnect_interval;   ///< 当前重连间隔(毫秒)
    bool _is_first_connection; ///< 标记是否为首次连接
    qint64 _last_pong_time;    ///< 上次收到 Pong 的时间戳 (毫秒)
    int _read_index;

    // 安全常量
    static const quint32 MAX_MESSAGE_LEN = 1024 * 1024; ///< 最大消息长度 1MB

public slots:
    /**
     * @brief 连接服务器槽函数
     * @param si 服务器信息结构体 (IP, Port, Token, Uid)
     */
    void slot_tcp_connect(ServerInfo si);

    /**
     * @brief 发送数据槽函数
     * @param reqId 请求类型 ID
     * @param data  发送的数据内容 (JSON 字符串)
     */
    void slot_send_data(RequestType reqId, const QString &data);

signals:
    /**
     * @brief 连接成功/失败信号
     * @param bsuccess true 表示连接成功，false 表示失败
     */
    void sig_con_success(bool bsuccess);

    /**
     * @brief 数据发送信号 (通常用于调试或日志)
     * @param reqId 请求 ID
     * @param data  发送的数据
     */
    void sig_send_data(RequestType reqId, QString data);

    /**
     * @brief 登录失败信号 (TCP 连接建立后的业务登录)
     * @param err 错误码
     */
    void sig_login_failed(int err);

    /**
     * @brief 收到完整消息信号
     * @param reqId 消息类型 ID
     * @param data  消息体数据
     */
    void sig_msg_received(RequestType reqId, QByteArray data);

    /**
     * @brief 收到消息信号 (quint16 版本)
     * @param msg_id 消息类型 ID
     * @param data   消息体数据
     */
    void sig_msg_received(quint16 msg_id, QByteArray data);

    /**
     * @brief 重连成功信号
     * @details 当断线后重新连接成功时发出，通知业务层重新发送登录包
     */
    void sig_reconnected();
};

#endif // TCPMGR_H

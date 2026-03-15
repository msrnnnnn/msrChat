/**
 * @file tcpmgr.cpp
 * @brief TCP 管理单例实现
 * @details 负责初始化网络线程与工作对象，转发外部发送/连接请求。
 */
#include "tcpmgr.h"
#include "tcpworker.h"
#include <QMetaObject>
#include <QMutexLocker>

QMutex TcpMgr::_mutex;
TcpMgr *TcpMgr::_instance = nullptr;

/**
 * @brief 获取单例实例
 * @return TcpMgr* 单例指针
 */
TcpMgr *TcpMgr::GetInstance()
{
    if (_instance)
    {
        return _instance;
    }
    QMutexLocker locker(&_mutex);
    if (!_instance)
    {
        _instance = new TcpMgr();
    }
    return _instance;
}

/**
 * @brief 销毁单例实例
 */
void TcpMgr::DestroyInstance()
{
    QMutexLocker locker(&_mutex);
    if (_instance)
    {
        delete _instance;
        _instance = nullptr;
    }
}

/**
 * @brief 构造函数
 * @param parent 父对象
 */
TcpMgr::TcpMgr(QObject *parent)
    : QObject(parent),
      _netThread(new QThread(this)),
      _worker(new TcpWorker())
{
    qRegisterMetaType<RequestType>("RequestType");
    qRegisterMetaType<ServerInfo>("ServerInfo");
    init_thread();
}

/**
 * @brief 析构函数
 */
TcpMgr::~TcpMgr()
{
    if (_worker)
    {
        QMetaObject::invokeMethod(_worker, "slot_stop", Qt::QueuedConnection);
        QMetaObject::invokeMethod(_worker, "deleteLater", Qt::QueuedConnection);
        _worker = nullptr;
    }
    if (_netThread)
    {
        _netThread->quit();
        _netThread->wait();
    }
}

/**
 * @brief 初始化网络线程与信号连接
 */
void TcpMgr::init_thread()
{
    _worker->moveToThread(_netThread);
    connect(_netThread, &QThread::started, _worker, &TcpWorker::slot_init);

    connect(_worker, &TcpWorker::sig_con_success, this, &TcpMgr::sig_con_success, Qt::QueuedConnection);
    connect(_worker, &TcpWorker::sig_reconnected, this, &TcpMgr::sig_reconnected, Qt::QueuedConnection);
    connect(
        _worker, static_cast<void (TcpWorker::*)(RequestType, QByteArray)>(&TcpWorker::sig_msg_received), this,
        static_cast<void (TcpMgr::*)(RequestType, QByteArray)>(&TcpMgr::sig_msg_received), Qt::QueuedConnection);
    connect(
        _worker, static_cast<void (TcpWorker::*)(quint16, QByteArray)>(&TcpWorker::sig_msg_received), this,
        static_cast<void (TcpMgr::*)(quint16, QByteArray)>(&TcpMgr::sig_msg_received), Qt::QueuedConnection);

    _netThread->start();
}

/**
 * @brief 发送连接请求到工作线程
 * @param si 服务器连接信息
 */
void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    if (!_worker)
    {
        return;
    }
    QMetaObject::invokeMethod(_worker, "slot_tcp_connect", Qt::QueuedConnection, Q_ARG(ServerInfo, si));
}

/**
 * @brief 发送数据到工作线程
 * @param reqId 请求类型
 * @param data 数据内容
 */
void TcpMgr::slot_send_data(RequestType reqId, const QString &data)
{
    if (!_worker)
    {
        return;
    }
    emit sig_send_data(reqId, data);
    QMetaObject::invokeMethod(
        _worker, "slot_send_data", Qt::QueuedConnection, Q_ARG(RequestType, reqId), Q_ARG(QString, data));
}

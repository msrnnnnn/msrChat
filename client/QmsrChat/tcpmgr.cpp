#include "tcpmgr.h"
#include "tcpworker.h"
#include <QMetaObject>
#include <QMutexLocker>

QMutex TcpMgr::_mutex;
TcpMgr *TcpMgr::_instance = nullptr;

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

void TcpMgr::DestroyInstance()
{
    QMutexLocker locker(&_mutex);
    if (_instance)
    {
        delete _instance;
        _instance = nullptr;
    }
}

TcpMgr::TcpMgr(QObject *parent)
    : QObject(parent),
      _netThread(new QThread(this)),
      _worker(new TcpWorker())
{
    qRegisterMetaType<RequestType>("RequestType");
    qRegisterMetaType<ServerInfo>("ServerInfo");
    init_thread();
}

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

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    if (!_worker)
    {
        return;
    }
    QMetaObject::invokeMethod(_worker, "slot_tcp_connect", Qt::QueuedConnection, Q_ARG(ServerInfo, si));
}

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

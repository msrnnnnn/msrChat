/**
 * @file    httpmanagement.cpp
 * @brief   HTTP 网络请求管理器实现
 * @details 封装了基于 Qt 的异步 HTTP POST 请求逻辑，支持单例模式与生命周期安全。
 */

#include "httpmanagement.h"
#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkReply>

HttpManagement::~HttpManagement()
{
    // 析构函数：Qt 的对象树机制或智能指针会自动处理资源
}

HttpManagement::HttpManagement()
{
    // 将通用的请求完成信号导流至逻辑分发槽
    connect(this, &HttpManagement::signal_http_finish, this, &HttpManagement::slot_http_finish);
}

void HttpManagement::PostHttpRequest(const QUrl &url, const QJsonObject &json, RequestType req_type, Modules mod)
{
    // 1. 序列化：将 JSON 对象转为紧凑格式的字节流，减少传输体积
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // 2. 构造请求头
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());

    // 3. 发起异步请求
    QNetworkReply *reply = _manager.post(request, data);

    // 4. 获取 shared_from_this 以确保在异步回调中对象存活
    auto self = shared_from_this();

    // 5. 绑定回调
    connect(
        reply, &QNetworkReply::finished,
        [self, reply, req_type, mod]()
        {
            // --- 异常处理阶段 ---
            if (reply->error() != QNetworkReply::NoError)
            {
                qWarning() << "Network Error captured: " << reply->errorString();
                emit self->signal_http_finish(req_type, "", ERRORCODES::ERROR_NETWORK, mod);
                reply->deleteLater();
                return;
            }

            QString res = QString::fromUtf8(reply->readAll());

            // 将成功获取的 JSON 字符串及业务状态发送给对应的 Dialog
            emit self->signal_http_finish(req_type, res, ERRORCODES::SUCCESS, mod);

            reply->deleteLater();
        });
}

void HttpManagement::registerHttpHandler(Modules mod, HttpHandler handler)
{
    _handlers.insert(mod, handler);
}

void HttpManagement::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    auto it = _handlers.find(mod);

    if (it != _handlers.end())
    {
        it.value()(req_type, res, err);
    }
    else
    {
        qWarning() << "Module handler not found. Module ID:" << static_cast<int>(mod);
    }
}




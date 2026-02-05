/**
 * @file    httpmanagement.cpp
 * @brief   HTTP 网络请求管理器实现
 * @details 封装了基于 Qt 的异步 HTTP POST 请求逻辑，支持单例模式与生命周期安全。
 * @author  msr
 */

#include "httpmanagement.h"
#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QNetworkReply>

// ============================================================
// 构造与析构
// ============================================================

HttpManagement::~HttpManagement()
{
    // 析构函数：由于 _manager 是成员变量而非指针，Qt 会自动处理其资源释放
}

HttpManagement::HttpManagement()
{
    // 将通用的请求完成信号导流至逻辑分发槽
    connect(this, &HttpManagement::signal_http_finish, this, &HttpManagement::slot_http_finish);
}

// ============================================================
// 核心业务逻辑
// ============================================================

/**
 * @brief 发送 HTTP POST 请求
 * @note 采用异步回调模式。通过 shared_from_this 延长生命周期，确保异步安全性。
 */
void HttpManagement::PostHttpRequest(const QUrl &url, const QJsonObject &json, RequestType req_type, Modules mod)
{
    // 1. 序列化：将 JSON 对象转为紧凑格式的字节流，准备网络传输
    // [优化] 使用 Compact 模式减少传输体积
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // 2. 构造请求头
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());

    // 3. 发起异步请求
    QNetworkReply *reply = _manager.post(request, data);

    // 4. 生命周期管理
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

/**
 * @brief 注册模块处理器
 */
void HttpManagement::registerHttpHandler(Modules mod, HttpHandler handler)
{
    _handlers.insert(mod, handler);
}

/**
 * @brief 内部信号分发槽
 */
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

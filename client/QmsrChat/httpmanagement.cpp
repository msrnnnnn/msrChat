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
#include <QPointer>

// ===========================================================
// 构造与析构
// ===========================================================

HttpManagement::~HttpManagement()
{
    // 析构函数：由于 _manager 是成员变量而非指针，Qt 会自动处理其资源释放
}

HttpManagement::HttpManagement()
{
    // 构造函数
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
    // ... 旧实现保持不变，为了兼容性 ...
    // 实际项目中可以考虑让它调用新实现，但这里逻辑不同(signal vs callback)，所以保留两套

    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());

    QNetworkReply *reply = _manager.post(request, data);
    auto self = shared_from_this();

    connect(
        reply, &QNetworkReply::finished,
        [self, reply, req_type, mod]()
        {
            if (reply->error() != QNetworkReply::NoError)
            {
                qWarning() << "Network Error:" << reply->errorString();
                emit self->signal_http_finish(req_type, "", ERRORCODES::ERR_NETWORK, mod);

                // [Legacy Support] 分发旧模块信号
                if (mod == Modules::REGISTER_MOD)
                    emit self->signal_register_mod_finish(req_type, "", ERRORCODES::ERR_NETWORK);
                if (mod == Modules::LOGIN_MOD)
                    emit self->sig_login_mod_finish(req_type, "", ERRORCODES::ERR_NETWORK);

                reply->deleteLater();
                return;
            }
            QString res = QString::fromUtf8(reply->readAll());
            emit self->signal_http_finish(req_type, res, ERRORCODES::SUCCESS, mod);

            // [Legacy Support] 分发旧模块信号
            if (mod == Modules::REGISTER_MOD)
                emit self->signal_register_mod_finish(req_type, res, ERRORCODES::SUCCESS);
            if (mod == Modules::LOGIN_MOD)
                emit self->sig_login_mod_finish(req_type, res, ERRORCODES::SUCCESS);

            reply->deleteLater();
        });
}

/**
 * @brief 发送 HTTP POST 请求 (Callback 模式)
 */
void HttpManagement::PostHttpRequest(
    const QUrl &url, const QJsonObject &json, QObject *receiver, HttpSuccessCallback success,
    HttpFailureCallback failure)
{
    // 1. 序列化
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // 2. 构造请求
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());

    // 3. 发起请求
    QNetworkReply *reply = _manager.post(request, data);

    // 4. 绑定回调 (使用 QPointer 监控 receiver 生命周期)
    // 技巧：我们连接到 HttpManagement 自身(单例，永远存在)，但在 Lambda 内部检查 receiver 是否存活
    // 这样既能保证 reply 总是被清理，又能防止回调悬垂指针

    QPointer<QObject> watcher(receiver);
    // [Fix C2665] connect 第三个参数不能是 shared_ptr，必须是裸指针
    auto self = shared_from_this(); // 保持引用计数
    QObject *context = self.get();

    connect(
        reply, &QNetworkReply::finished, context,
        [self, reply, watcher, success, failure]() // self 被捕获，保证 lambda 执行时对象存活
        {
            // 无论如何，reply 都要清理
            reply->deleteLater();

            // 检查接收者是否还活着
            if (watcher.isNull())
            {
                qDebug() << "Http Callback dropped: Receiver destroyed.";
                return;
            }

            // --- 异常处理 ---
            if (reply->error() != QNetworkReply::NoError)
            {
                qWarning() << "Network Error:" << reply->errorString();
                if (failure)
                {
                    failure(ERRORCODES::ERR_NETWORK);
                }
                return;
            }

            // --- 读取数据 ---
            QByteArray responseData = reply->readAll();

            // --- JSON 解析 ---
            QJsonParseError jsonError;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &jsonError);

            if (jsonError.error != QJsonParseError::NoError || !jsonDoc.isObject())
            {
                qWarning() << "JSON Parse Error:" << jsonError.errorString();
                if (failure)
                {
                    failure(ERRORCODES::ERR_JSON);
                }
                return;
            }

            // --- 成功回调 ---
            if (success)
            {
                success(jsonDoc.object());
            }
        });
}

// 移除 registerHttpHandler 和 slot_http_finish 的实现

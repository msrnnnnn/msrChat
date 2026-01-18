/**
 * @file    httpmanagement.cpp
 * @brief   HTTP 网络请求管理器实现
 * @details 封装了基于 Qt 的异步 HTTP POST 请求逻辑，支持单例模式与生命周期安全。
 * [架构升级] 2026/01/17 引入注册表模式，实现模块解耦。
 * @author  msr
 * @date    2026/01/06
 */

#include "httpmanagement.h"
#include <QByteArray>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QDebug>

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
    // 采用“总线型”设计，先统一接收，再由 slot_http_finish 进行查表分发
    connect(this, &HttpManagement::signal_http_finish, this, &HttpManagement::slot_http_finish);
}

// ============================================================
// 核心业务逻辑
// ============================================================

/**
 * @brief 发送 HTTP POST 请求
 * @note 采用异步回调模式。通过 shared_from_this 延长生命周期，确保异步安全性。
 */
void HttpManagement::PostHttpRequest(const QUrl& url, const QJsonObject& json, RequestType req_type, Modules mod)
{
    // 1. 序列化：将 JSON 对象转为紧凑格式的字节流，准备网络传输
    // [优化] 使用 Compact 模式减少传输体积 (Traffic Optimization)
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // 2. 构造请求头：明确告知服务器数据格式和长度
    // 这是工业级规范，能避免某些 Web 服务器 (如 Nginx/IIS) 由于缺少 Content-Length 而拒绝处理请求
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());

    // 3. 发起异步请求：post 会立即返回一个处理句柄(Reply)，不会阻塞界面
    QNetworkReply *reply = _manager.post(request, data);

    // 4. 生命周期管理 (Lifetime Management)
    // [核心要点] 通过智能指针 self 延长单例的生命周期。(保活)
    // 确保在 Lambda 回调执行时 (可能几百毫秒后)，HttpManagement 实例依然有效，防止野指针崩溃。
    auto self = shared_from_this();

    // 5. 绑定回调：当网络请求完成（无论成功或失败）时触发
    // 捕获列表 [self, reply, req_type, mod] 确保了回调函数拥有必要的上下文数据
    connect(reply, &QNetworkReply::finished, [self, reply, req_type, mod](){

        // --- 异常处理阶段 ---
        // 检查网络链路层错误（如 DNS 解析失败、连接拒绝等）
        if(reply->error() != QNetworkReply::NoError){
            // [优化] 错误记录提升为 Warning 级别，方便 Release 版本排查问题
            qWarning() << "Network Error captured: " << reply->errorString();

            // 向上层反馈网络异常状态
            emit self->signal_http_finish(req_type, "", ERRORCODES::ERROR_NETWORK, mod);

            // [安全释放] 必须使用 deleteLater() 确保在当前信号处理结束后释放内存。放入 Qt 事件循环队列
            reply->deleteLater();
            return;
        }

        // [优化] 显式使用 FromUtf8 编码读取，防止 Windows 环境下中文乱码。
        QString res = QString::fromUtf8(reply->readAll());

        // 将成功获取的 JSON 字符串及业务状态发送给对应的 Dialog
        emit self->signal_http_finish(req_type, res, ERRORCODES::SUCCESS, mod);

        reply->deleteLater();
    });
}

/**
 * @brief 注册模块处理器 (Registry Pattern)
 * @note  这是“开闭原则”的核心实现。允许各业务模块在初始化时“订阅”自己的网络回包，
 * 从而将 HttpManagement 与具体业务逻辑彻底解耦。
 */
void HttpManagement::registerHttpHandler(Modules mod, HttpHandler handler)
{
    // 将模块 ID 与对应的回调函数存入 Hash Map
    // 注意：如果同一模块多次注册，insert 会覆盖旧的 handler
    _handlers.insert(mod, handler);
}

/**
 * @brief 内部信号分发槽
 * @param mod 模块标识，用于查找对应的处理器
 */
void HttpManagement::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    // [分发逻辑优化] 查表分发 (Map Dispatch)
    // 相比原来的 if-else 链，这里的时间复杂度从 O(N) 优化到了 O(logN) 或 O(1)
    auto it = _handlers.find(mod);

    // 工业级健壮性检查：防止未注册的模块导致崩溃或静默失败
    if(it != _handlers.end()){
        // 找到处理器 -> 执行回调
        // 这里的 value() 就是我们注册的 std::function (通常是 Lambda)
        it.value()(req_type, res, err);
    }else{
        // 异常捕获：如果某个模块发了请求但没注册处理器，打印警告
        qWarning() << "Module handler not found. Module ID:" << static_cast<int>(mod);
    }
}

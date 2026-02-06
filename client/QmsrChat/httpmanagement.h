/**
 * @file    httpmanagement.h
 * @brief   HTTP 网络请求管理器 (基于 Qt Network)
 * @author  msr
 */

#ifndef HTTPMANAGEMENT_H
#define HTTPMANAGEMENT_H

#include "Singleton.h"
#include "global.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

// 前置声明枚举类型 (已在 global.h 定义，此处保留或移除皆可，建议移除避免冲突)
// enum class RequestType;
// enum class ERRORCODES;
// enum class Modules;

// 回调函数类型定义
// 成功回调：传入解析后的 JSON 对象
using HttpSuccessCallback = std::function<void(const QJsonObject &)>;
// 失败回调：传入错误码
using HttpFailureCallback = std::function<void(ERRORCODES)>;

/**
 * @class   HttpManagement
 * @brief   全局 HTTP 请求管理单例类
 * @details
 * 1. 负责统一发送 HTTP POST 请求。
 * 2. 继承 std::enable_shared_from_this 以支持在异步回调中安全获取自身的智能指针。
 * 3. 继承 Singleton 实现全局单例访问。
 * @warning 本类继承自 QObject，必须在 Qt 主线程或具有 QEventLoop 的线程中实例化。
 */
class HttpManagement : public QObject,
                       public Singleton<HttpManagement>,
                       public std::enable_shared_from_this<HttpManagement>
{
    Q_OBJECT

public:
    // 允许(父类)Singleton 模板访问HttpManagement私有构造函数
    friend class Singleton<HttpManagement>;

    ~HttpManagement();

    /**
     * @brief 发送 HTTP POST 请求 (Callback 模式)
     * @param url       目标 URL
     * @param json      请求体数据
     * @param receiver  上下文对象 (用于生命周期管理，通常是 this)
     * @param success   成功回调
     * @param failure   失败回调 (可选)
     */
    void PostHttpRequest(
        const QUrl &url, const QJsonObject &json, QObject *receiver, HttpSuccessCallback success,
        HttpFailureCallback failure = nullptr);

    /**
     * @brief 发送 HTTP POST 请求 (Signal-Slot 遗留模式)
     * @deprecated 请使用 Callback 模式
     */
    void PostHttpRequest(const QUrl &url, const QJsonObject &json, RequestType req_type, Modules mod);

private:
    /**
     * @brief 私有构造函数 (单例模式)
     */
    HttpManagement();

    /**
     * @brief Qt 网络访问管理器
     * @note  这是异步网络的核心，依赖 Qt 事件循环。
     */
    QNetworkAccessManager _manager;

signals:
    /**
     * @brief 信号：HTTP 请求完成 (通知业务层)
     * @param req_type  原样返回的请求 ID
     * @param res       服务器返回的原始数据 (JSON 字符串)
     * @param err       错误码
     * @param mod       模块 ID
     */
    void signal_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod);

    /**
     * @brief 信号：(遗留接口) 注册模块专用完成信号
     * @deprecated 建议未来统一使用 signal_http_finish 分发
     */
    void signal_register_mod_finish(RequestType req_type, QString res, ERRORCODES err);

    /**
     * @brief 信号：(遗留接口) 登录模块专用完成信号
     * @deprecated 建议未来统一使用 signal_http_finish 分发
     */
    void sig_login_mod_finish(ReqId id, QString res, ErrorCodes err);
};

#endif // HTTPMANAGEMENT_H

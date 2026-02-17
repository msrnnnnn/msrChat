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
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>

// 前置声明枚举类型(减少头文件依赖，减少编译时间)
enum class RequestType;
enum class ERRORCODES;
enum class Modules;

using HttpHandler = std::function<void(RequestType, QString, ERRORCODES)>;

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
     * @brief 发送 HTTP POST 请求
     * @param url       目标 URL 地址
     * @param json      请求体数据 (JSON 格式)
     * @param req_type  请求类型 ID (用于回调时区分是哪个请求，如 ID_GET_VARIFY_CODE)
     * @param mod       发起请求的模块 ID (用于将回包分发给 Login/Register 等不同模块)
     * * @note 已优化为 const reference 传递，避免拷贝开销。
     */
    void PostHttpRequest(const QUrl &url, const QJsonObject &json, RequestType req_type, Modules mod);

    /**
     * @brief 注册业务模块处理器
     * @param mod      业务模块 ID (Key)
     * @param handler  对应的回调函数 (Value)
     * @note  各业务模块 (Dialog) 初始化时调用此接口，订阅属于自己的网络回包。
     */
    void registerHttpHandler(Modules mod, HttpHandler handler);

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

    /**
     * @brief 业务处理器注册表
     * @note  Key: 模块ID -> Value: 回调函数。替代了传统的 switch-case 分发。
     */
    QMap<Modules, HttpHandler> _handlers;

private slots:
    /**
     * @brief 内部槽函数：处理网络请求完成信号
     * @details
     * 连接到 QNetworkReply::finished 信号。
     * 负责读取服务器响应数据、解析 JSON、检查网络层错误，并转发信号给业务层。
     * @param req_type 请求类型
     * @param res      响应数据
     * @param err      错误码
     * @param mod      模块 ID
     */
    void slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod);

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

    void sig_login_mod_finish(RequestType id, QString res, ERRORCODES err, Modules mod);
};

#endif // HTTPMANAGEMENT_H

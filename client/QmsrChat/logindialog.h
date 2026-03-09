/**
 * @file logindialog.h
 * @brief 登录对话框类
 * @details 负责处理用户登录逻辑、表单校验及与服务器的交互。
 */
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "global.h"
#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <functional>

namespace Ui
{
    class LoginDialog;
}

/**
 * @class LoginDialog
 * @brief 登录界面对话框
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private:
    Ui::LoginDialog *ui;

    /**
     * @brief 校验用户名格式
     * @return true 格式正确
     */
    bool checkUserValid();

    /**
     * @brief 校验密码格式
     * @return true 格式正确
     */
    bool checkPwdValid();

    /**
     * @brief 显示提示信息
     * @param str 提示内容
     * @param isCorrect true显示正常颜色，false显示错误颜色
     */
    void showTip(QString str, bool isCorrect);

    /**
     * @brief 初始化网络回包处理器
     */
    void initHandlers();

private slots:
    /**
     * @brief 登录按钮点击槽
     */
    void on_login_Button_clicked();

    /**
     * @brief 处理 HTTP 请求完成信号
     * @param req_type 请求类型
     * @param res      响应数据
     * @param err      错误码
     * @param mod      模块 ID
     */
    void slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod);

    /**
     * @brief 忘记密码标签点击槽
     */
    void slot_forget_pwd();

    /**
     * @brief TCP 连接建立完成槽
     * @param bsuccess 连接是否成功
     */
    void slot_tcp_con_finish(bool bsuccess);

signals:
    /**
     * @brief 切换到注册界面信号
     */
    void switchRegister();

    /**
     * @brief 切换到重置密码界面信号
     */
    void switchReset();

    /**
     * @brief 请求建立 TCP 连接信号
     * @param si 服务器信息
     */
    void sig_connect_tcp(ServerInfo si);

private:
    /**
     * @brief 业务逻辑处理器映射表
     * @details Key: 请求类型, Value: 处理函数
     */
    QMap<RequestType, std::function<void(const QJsonObject &)>> _handlers;
    
    int _uid = 0;       ///< 用户 ID
    QString _token;     ///< 登录令牌
};

#endif // LOGINDIALOG_H

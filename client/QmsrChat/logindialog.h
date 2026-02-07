/**
 * @file logindialog.h
 * @brief 登录对话框类
 * @author msr
 */

#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "global.h"
#include <QDialog>

namespace Ui
{
class LoginDialog;
}

/**
 * @class LoginDialog
 * @brief 登录界面对话框
 * @details 处理用户登录交互，包含账号密码输入及跳转注册。
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit LoginDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~LoginDialog();

private:
    Ui::LoginDialog *ui; ///< UI 界面指针

    /**
     * @brief 校验用户名有效性
     * @return bool
     */
    bool checkUserValid();

    /**
     * @brief 校验密码有效性
     * @return bool
     */
    bool checkPwdValid();

    /**
     * @brief 显示错误提示
     * @param str 提示内容
     * @param b_ok 是否为成功提示(true显示绿色/默认色, false显示红色)
     */
    void showTip(QString str, bool b_ok);

    /**
     * @brief 设置登录按钮是否可用
     * @param enabled true:可用, false:不可用
     */
    void enableBtn(bool enabled);

    /**
     * @brief 初始化 HTTP 处理器
     */
    void initHttpHandlers();

    // HTTP 处理器注册表
    QMap<ReqId, std::function<void(const QJsonObject &)>> _handlers;

    int _uid;
    QString _token;

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
     * @brief 连接 TCP 服务器信号
     */
    void sig_connect_tcp(ServerInfo si);

private slots:
    void slot_forget_pwd();
    void on_login_Button_clicked();
    void slot_login_mod_finish(ReqId id, QString res, ErrorCodes err);
    void slot_tcp_con_finish(bool bsuccess);
    void slot_login_failed(int err);
};

#endif // LOGINDIALOG_H

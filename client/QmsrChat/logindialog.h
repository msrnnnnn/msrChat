/**
 * @file logindialog.h
 * @brief 登录对话框类
 * @author msr
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

    bool checkUserValid();
    bool checkPwdValid();
    void showTip(QString str, bool isCorrect);
    void initHandlers();

private slots:
    void on_login_Button_clicked();
    void slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod);
    void slot_forget_pwd();
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
    void sig_connect_tcp(ServerInfo si);

private:
    QMap<RequestType, std::function<void(const QJsonObject &)>> _handlers;
    int _uid = 0;
    QString _token;
};

#endif // LOGINDIALOG_H

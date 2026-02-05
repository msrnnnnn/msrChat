/**
 * @file logindialog.h
 * @brief 登录对话框类
 * @author msr
 */

#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

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

signals:
    /**
     * @brief 切换到注册界面信号
     */
    void switchRegister();
};

#endif // LOGINDIALOG_H

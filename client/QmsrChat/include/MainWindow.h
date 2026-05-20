/**
 * @file    MainWindow.h
 * @brief   主窗口类
 * @details 负责管理应用程序的主界面及子界面切换（登录、注册、重置密码）。
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ChatDialog.h"
#include "LoginDialog.h"
#include "RegisterDialog.h"
#include "ResetDialog.h"
#include <QCloseEvent>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief 应用程序主窗口
 * @details 管理登录和注册对话框的切换和显示。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainWindow();

public slots:
    /**
     * @brief 切换到注册界面
     */
    void slotSwitchRegister();

    /**
     * @brief 切换回登录界面
     */
    void slotSwitchLogin();

    /**
     * @brief 切换到重置密码界面
     */
    void slotSwitchReset();

    /**
     * @brief 处理登录成功，切换到聊天界面
     */
    void slotLoginSuccess();

    /**
     * @brief 处理 Token 失效，回退到登录界面
     */
    void slotTokenInvalid();

private:
    /**
     * @brief 关闭事件
     * @details 统一在点击窗口关闭按钮时退出整个程序。
     */
    void closeEvent(QCloseEvent *event) override;

    Ui::MainWindow *ui; ///< UI 指针

    LoginDialog *_login_dialog;       ///< 登录对话框实例
    RegisterDialog *_register_dialog; ///< 注册对话框实例
    ResetDialog *_reset_dialog;       ///< 重置对话框实例
    ChatDialog *_chat_dialog;         ///< 聊天对话框实例
};

#endif // MAINWINDOW_H

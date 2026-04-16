/**
 * @file LoginDialog.h
 * @brief 登录对话框类
 * @details 负责处理用户登录逻辑、表单校验及与服务器的交互。
 */
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "ProtocolStructs.h"
#include "Global.h"
#include <QDialog>

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
     * @brief 登录回包处理槽
     * @param rsp 强类型登录响应结构体
     */
    void slot_login_rsp(const LoginRspStruct &rsp);

    /**
     * @brief 忘记密码标签点击槽
     */
    void slot_forget_pwd();

    /**
     * @brief TCP 连接建立完成槽
     * @param bsuccess 连接是否成功
     */
    void slot_tcp_con_finish(bool bsuccess);

    /**
     * @brief 聊天登录回包处理槽
     * @param rsp 强类型聊天登录响应结构体
     */
    void slot_chat_login_rsp(const ChatLoginRspStruct &rsp);

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
     * @brief 登录成功信号
     * @details 通知主窗口登录成功，可以切换到聊天界面
     */
    void sig_login_success();

private:
    int _uid = 0;
    QString _token;
    bool _chat_login_ready = false;
};

#endif // LOGINDIALOG_H

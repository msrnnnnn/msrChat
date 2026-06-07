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
#include <QVector>

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

    /**
     * @brief 取出聊天登录前缓冲的消息
     * @details 重连场景下，登录前到达的消息会被暂存，登录成功后一次性取出。
     * @return 缓冲的聊天消息列表
     */
    QVector<ChatTextMsgStruct> TakeBufferedMessages();

protected:
    /**
     * @brief 原生系统事件处理
     * @details 用于拦截 Windows 原生消息（如任务栏重绘事件）。
     * @param eventType 事件类型
     * @param message 原生消息指针
     * @param result 处理结果
     * @return true 事件已被处理
     */
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

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
     * @brief 请求切换到注册界面
     */
    void switchRegister();
    /**
     * @brief 请求切换到重置密码界面
     */
    void switchReset();
    /**
     * @brief 登录成功信号
     */
    void sig_login_success();
    /**
     * @brief Token 失效信号
     */
    void sig_token_invalid();

private:
    int _uid = 0;                                          ///< 当前用户ID
    QString _token;                                        ///< 登录令牌
    bool _chat_login_ready = false;                        ///< 聊天登录是否已完成
    QVector<ChatTextMsgStruct> _buffered_messages;         ///< 聊天登录前缓冲的消息
    QMetaObject::Connection _msg_buffer_connection;        ///< 消息缓冲信号连接句柄
};

#endif // LOGINDIALOG_H

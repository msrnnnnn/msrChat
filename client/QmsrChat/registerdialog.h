/**
 * @file    registerdialog.h
 * @brief   注册对话框类
 * @author  msr
 */

#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include "global.h"
#include <QDialog>
#include <QMap>
#include <functional>

namespace Ui
{
class RegisterDialog;
}

/**
 * @class RegisterDialog
 * @brief 用户注册交互界面
 * @details 负责处理用户注册流程，包括邮箱格式校验、验证码获取请求、注册信息提交等。
 * 采用 Map 注册表模式处理 HTTP 回包，避免由 switch-case 带来的逻辑膨胀。
 */
class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit RegisterDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~RegisterDialog();

signals:
    /**
     * @brief 切换到登录界面信号
     * @note 当用户点击“取消”或注册成功后触发
     */
    void switchLogin();

private slots:
    /**
     * @brief 确认注册按钮点击槽函数
     */
    void on_Confirm_Button_clicked();

    /**
     * @brief 获取验证码按钮点击槽函数
     * @details 执行邮箱正则校验，通过后请求验证码
     */
    void on_confirm_verifycode_Button_clicked();

private:
    /**
     * @brief 显示提示信息 (错误/成功)
     * @param str 提示文本
     * @param isCorrect true=绿色/正常状态, false=红色/错误状态
     */
    void showTip(QString str, bool isCorrect);

    Ui::RegisterDialog *ui; ///< UI 界面指针
};

#endif // REGISTERDIALOG_H

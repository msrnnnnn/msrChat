/**
 * @file resetdialog.h
 * @brief 重置密码对话框类
 * @author msr
 */

#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include "global.h"
#include <QDialog>
#include <QMap>

namespace Ui
{
class ResetDialog;
}

/**
 * @class ResetDialog
 * @brief 重置密码交互界面
 * @details 处理用户忘记密码时的重置流程，包括邮箱验证码获取和新密码设置。
 */
class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit ResetDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ResetDialog();

private slots:
    /**
     * @brief 获取验证码按钮点击槽函数
     */
    void on_verify_btn_clicked();

    /**
     * @brief 确认重置按钮点击槽函数
     */
    void on_sure_btn_clicked();

    /**
     * @brief 取消按钮点击槽函数
     */
    void on_cancel_btn_clicked();

signals:
    /**
     * @brief 切换到登录界面信号
     */
    void switchLogin();

private:
    /**
     * @brief 校验用户名格式
     * @return true 校验通过, false 校验失败
     */
    bool checkUserValid();

    /**
     * @brief 校验密码格式
     * @return true 校验通过, false 校验失败
     */
    bool checkPassValid();

    /**
     * @brief 校验邮箱格式
     * @return true 校验通过, false 校验失败
     */
    bool checkEmailValid();

    /**
     * @brief 校验验证码非空
     * @return true 校验通过, false 校验失败
     */
    bool checkVarifyValid();

    /**
     * @brief 添加错误提示
     * @param te 错误类型
     * @param tips 提示文本
     */
    void AddTipErr(TipErr te, QString tips);

    /**
     * @brief 移除错误提示
     * @param te 错误类型
     */
    void DelTipErr(TipErr te);

    /**
     * @brief 显示提示信息
     * @param str 提示文本
     * @param b_ok true=成功/绿色, false=失败/红色
     */
    void showTip(QString str, bool b_ok);

    Ui::ResetDialog *ui;             ///< UI 界面指针
    QMap<TipErr, QString> _tip_errs; ///< 错误信息缓存
};

#endif // RESETDIALOG_H

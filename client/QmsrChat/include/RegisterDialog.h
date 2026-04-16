/**
 * @file    RegisterDialog.h
 * @brief   注册对话框类
 * @details 负责用户注册流程，包含输入校验、验证码获取及提交注册信息。
 */
#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include "ProtocolStructs.h"
#include "Global.h"
#include <QDialog>

class QTimer;

namespace Ui
{
class RegisterDialog;
}

/**
 * @class RegisterDialog
 * @brief 用户注册交互界面
 * @details
 * 1. 采用 Map 注册表模式处理 HTTP 回包，避免 switch-case 逻辑膨胀。
 * 2. 包含完整的表单校验逻辑（用户名、邮箱、密码强度、验证码）。
 */
class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

signals:
    /**
     * @brief 切换到登录界面信号
     */
    void switchLogin();

private slots:
    /**
     * @brief 确认注册按钮点击槽
     */
    void on_Confirm_Button_clicked();

    /**
     * @brief 获取验证码按钮点击槽
     */
    void on_confirm_verifycode_Button_clicked();

    /**
     * @brief 验证码响应处理槽
     * @param rsp 验证码响应结构体
     */
    void slot_verify_code_rsp(const VerifyCodeRspStruct &rsp);

    /**
     * @brief 注册响应处理槽
     * @param rsp 注册响应结构体
     */
    void slot_register_rsp(const RegisterRspStruct &rsp);

    /**
     * @brief 返回按钮点击槽
     */
    void on_return_btn_clicked();

    /**
     * @brief 取消按钮点击槽
     */
    void on_Cancel_Button_clicked();

private:
    /**
     * @brief 开始验证码倒计时
     * @param seconds 倒计时秒数
     */
    void startVerifyCountdown(int seconds);

    /**
     * @brief 更新错误提示信息显示
     */
    void ChangeTipPage();

    /* 表单校验函数组 */
    bool checkUserValid();    ///< 校验用户名
    bool checkEmailValid();   ///< 校验邮箱
    bool checkPassValid();    ///< 校验密码
    bool checkConfirmValid(); ///< 校验确认密码
    bool checkVarifyValid();  ///< 校验验证码

    /**
     * @brief 添加错误提示
     * @param te 错误类型
     * @param tips 提示内容
     */
    void AddTipErr(TipErr te, QString tips);

    /**
     * @brief 移除错误提示
     * @param te 错误类型
     */
    void DelTipErr(TipErr te);

    /**
     * @brief 显示提示信息
     * @param str 提示内容
     * @param isCorrect true显示正常颜色，false显示错误颜色
     */
    void showTip(QString str, bool isCorrect);

    Ui::RegisterDialog *ui;

    QMap<TipErr, QString> _tip_errs;
    QTimer *_countdown_timer;
    int _countdown;
};

#endif // REGISTERDIALOG_H

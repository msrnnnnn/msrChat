/**
 * @file    ResetDialog.h
 * @brief   重置密码对话框类
 * @details 负责用户重置密码的交互逻辑。
 */
#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include "Global.h"
#include "ProtocolStructs.h"
#include <QDialog>
#include <QMap>

namespace Ui
{
class ResetDialog;
}

/**
 * @brief 重置密码对话框
 * @details 提供用户重置密码的完整流程，包含验证码获取、表单校验和服务端交互。
 */
class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

signals:
    /**
     * @brief 请求切换到登录界面
     */
    void switchLogin();

private slots:
    void on_varify_btn_clicked();
    void on_sure_btn_clicked();
    /**
     * @brief 处理服务端下发的验证码响应
     * @param rsp 验证码响应结构体
     */
    void slot_verify_code_rsp(const VerifyCodeRspStruct &rsp);
    /**
     * @brief 处理服务端下发的重置密码响应
     * @param rsp 重置密码响应结构体
     */
    void slot_reset_pwd_rsp(const ResetPwdRspStruct &rsp);

private:
    Ui::ResetDialog *ui;
    QMap<TipErr, QString> _tip_errs;
};

#endif // RESETDIALOG_H

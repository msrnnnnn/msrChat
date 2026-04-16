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

class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

signals:
    void switchLogin();

private slots:
    void on_varify_btn_clicked();
    void on_sure_btn_clicked();
    void slot_verify_code_rsp(const VerifyCodeRspStruct &rsp);
    void slot_reset_pwd_rsp(const ResetPwdRspStruct &rsp);

private:
    bool checkUserValid();
    bool checkEmailValid();
    bool checkPassValid();
    bool checkVarifyValid();
    void showTip(QString str, bool isCorrect);
    void AddTipErr(TipErr te, QString tips);
    void DelTipErr(TipErr te);

    Ui::ResetDialog *ui;
    QMap<TipErr, QString> _tip_errs;
};

#endif // RESETDIALOG_H

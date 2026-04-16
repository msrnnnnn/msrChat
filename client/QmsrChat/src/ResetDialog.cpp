/**
 * @file    ResetDialog.cpp
 * @brief   重置密码对话框实现
 */

#include "ResetDialog.h"
#include "TcpMgr.h"
#include "ui_resetdialog.h"
#include <QRegularExpression>

/**
 * @brief 构造函数
 * @param parent 父窗口
 */
ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ResetDialog)
{
    ui->setupUi(this);

    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label);

    connect(ui->cancel_btn, &QPushButton::clicked, this, &ResetDialog::switchLogin);
    connect(TcpMgr::Instance(), &TcpMgr::sig_verify_code_rsp, this, &ResetDialog::slot_verify_code_rsp);
    connect(TcpMgr::Instance(), &TcpMgr::sig_reset_pwd_rsp, this, &ResetDialog::slot_reset_pwd_rsp);

    connect(ui->user_edit, &QLineEdit::editingFinished, this, [this]() { checkUserValid(); });
    connect(ui->email_edit, &QLineEdit::editingFinished, this, [this]() { checkEmailValid(); });
    connect(ui->pwd_edit, &QLineEdit::editingFinished, this, [this]() { checkPassValid(); });
    connect(ui->varify_edit, &QLineEdit::editingFinished, this, [this]() { checkVarifyValid(); });
    ui->varify_btn->setAutoStart(false);
}

/**
 * @brief 析构函数
 */
ResetDialog::~ResetDialog()
{
    delete ui;
}

/**
 * @brief 校验用户名合法性
 * @return bool 是否有效
 */
bool ResetDialog::checkUserValid()
{
    if (ui->user_edit->text().isEmpty())
    {
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

/**
 * @brief 校验密码合法性
 * @return bool 是否有效
 */
bool ResetDialog::checkPassValid()
{
    auto pass = ui->pwd_edit->text();
    if (pass.length() < 6 || pass.length() > 15)
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    if (!regExp.match(pass).hasMatch())
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }
    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}

/**
 * @brief 校验邮箱合法性
 * @return bool 是否有效
 */
bool ResetDialog::checkEmailValid()
{
    auto email = ui->email_edit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    if (!regex.match(email).hasMatch())
    {
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

/**
 * @brief 校验验证码合法性
 * @return bool 是否有效
 */
bool ResetDialog::checkVarifyValid()
{
    auto pass = ui->varify_edit->text();
    if (pass.isEmpty())
    {
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

/**
 * @brief 获取验证码按钮点击处理
 */
void ResetDialog::on_varify_btn_clicked()
{
    if (!checkEmailValid())
    {
        return;
    }

    VerifyCodeReqStruct req;
    req.email = ui->email_edit->text();
    TcpMgr::Instance()->slot_send_verify_code_req(req);
}

/**
 * @brief 确认重置按钮点击处理
 */
void ResetDialog::on_sure_btn_clicked()
{
    bool valid = checkUserValid();
    if (!valid)
        return;
    valid = checkEmailValid();
    if (!valid)
        return;
    valid = checkPassValid();
    if (!valid)
        return;
    valid = checkVarifyValid();
    if (!valid)
        return;

    ResetPwdReqStruct req;
    req.user = ui->user_edit->text();
    req.email = ui->email_edit->text();
    req.passwd = hashPassword(ui->pwd_edit->text());
    req.varifycode = ui->varify_edit->text();
    TcpMgr::Instance()->slot_send_reset_pwd_req(req);
}

void ResetDialog::slot_verify_code_rsp(const VerifyCodeRspStruct &rsp)
{
    if (!this->isVisible())
    {
        return;
    }

    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS))
    {
        showTip(tr("获取验证码失败"), false);
        return;
    }

    showTip(tr("验证码已发送到邮箱，注意查收"), true);
    ui->varify_btn->startCountdown(10);
}

void ResetDialog::slot_reset_pwd_rsp(const ResetPwdRspStruct &rsp)
{
    if (!this->isVisible())
    {
        return;
    }

    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS))
    {
        QString errStr = tr("重置失败");
        switch (static_cast<ERRORCODES>(rsp.error))
        {
            case ERRORCODES::VarifyCodeExpired:
                errStr = tr("验证码已过期");
                break;
            case ERRORCODES::VarifyCodeErr:
                errStr = tr("验证码错误");
                break;
            case ERRORCODES::EmailNotMatch:
                errStr = tr("用户名与邮箱不匹配");
                break;
            case ERRORCODES::PasswdUpFailed:
                errStr = tr("密码更新失败");
                break;
            default:
                break;
        }
        showTip(errStr, false);
        return;
    }

    showTip(tr("重置成功，返回登录"), true);
}

/**
 * @brief 记录输入错误并提示
 * @param te 错误类型
 * @param tips 提示文本
 */
void ResetDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips, false);
}

/**
 * @brief 移除输入错误并刷新提示
 * @param te 错误类型
 */
void ResetDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if (_tip_errs.empty())
    {
        ui->error_label->setProperty("state", "normal");
        ui->error_label->setText("");
        repolish(ui->error_label);
        return;
    }
    showTip(_tip_errs.first(), false);
}

/**
 * @brief 显示提示信息
 * @param str 提示文本
 * @param isCorrect 是否为成功提示
 */
void ResetDialog::showTip(QString str, bool isCorrect)
{
    if (isCorrect)
    {
        ui->error_label->setProperty("state", "normal");
    }
    else
    {
        ui->error_label->setProperty("state", "error");
    }
    ui->error_label->setText(str);
    repolish(ui->error_label);
}

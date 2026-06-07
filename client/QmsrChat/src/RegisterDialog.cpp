/**
 * @file    RegisterDialog.cpp
 * @brief   注册对话框实现
 * @note    此对话框依赖 main.cpp 启动时建立的全局 TCP 长连接。
 *          所有请求通过 TcpMgr 单例发送，无需单独建立连接。
 */
#include "AuthUiHelpers.h"
#include "RegisterDialog.h"
#include "TcpMgr.h"
#include "Utils.h"
#include "ui_registerdialog.h"
#include <QDebug>
#include <QTimer>

/**
 * @brief 构造函数
 * @details 初始化 UI、连接信号槽、初始化 HTTP 处理器。
 */
RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    ui->error_label->setProperty("state", "normal");
    Utils::repolish(ui->error_label);

    connect(TcpMgr::Instance(), &TcpMgr::sigVerifyCodeRsp, this, &RegisterDialog::slot_verify_code_rsp);
    connect(TcpMgr::Instance(), &TcpMgr::sigRegisterRsp, this, &RegisterDialog::slot_register_rsp);

    // 连接各输入框的 editingFinished 信号，实时校验输入合法性
    connect(ui->user_Edit, &QLineEdit::editingFinished, this, [this]() {
        AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_USER_ERR,
            AuthUiHelpers::ValidateUsername(ui->user_Edit->text()), ui->error_label);
    });
    connect(ui->email_Edit, &QLineEdit::editingFinished, this, [this]() {
        AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_EMAIL_ERR,
            AuthUiHelpers::ValidateEmail(ui->email_Edit->text()), ui->error_label);
    });
    connect(ui->password_Edit, &QLineEdit::editingFinished, this, [this]() {
        AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_PWD_ERR,
            AuthUiHelpers::ValidatePassword(ui->password_Edit->text()), ui->error_label);
    });
    connect(ui->confirm_password_Edit, &QLineEdit::editingFinished, this, [this]() { checkConfirmValid(); });
    connect(ui->verifycode_Edit, &QLineEdit::editingFinished, this, [this]() {
        AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_VARIFY_ERR,
            AuthUiHelpers::ValidateVerifyCode(ui->verifycode_Edit->text()), ui->error_label);
    });

    AuthUiHelpers::BindPasswordToggle(ui->pass_visible, ui->password_Edit);
    AuthUiHelpers::BindPasswordToggle(ui->confirm_visible, ui->confirm_password_Edit);

    ui->confirm_verifycode_Button->setAutoStart(false);

    _countdown_timer = new QTimer(this);
    _countdown = 5;
    // 注册成功后倒计时，倒计时结束自动切回登录页
    connect(
        _countdown_timer, &QTimer::timeout, this,
        [this]()
        {
            if (_countdown <= 0)
            {
                _countdown_timer->stop();
    // 默认显示注册表单页（page_1）
    ui->stackedWidget->setCurrentWidget(ui->page_1);
                emit switchLogin();
                return;
            }
            _countdown -= 1;
            auto str = tr("注册成功，%1 s后返回登录").arg(_countdown);
            ui->tip_lb->setText(str);
        });
    ui->stackedWidget->setCurrentWidget(ui->page_1);
}

/**
 * @brief 析构函数
 */
RegisterDialog::~RegisterDialog()
{
    delete ui;
}

/**
 * @brief 确认注册按钮点击处理
 */
void RegisterDialog::on_Confirm_Button_clicked()
{
    if (!AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_USER_ERR,
            AuthUiHelpers::ValidateUsername(ui->user_Edit->text()), ui->error_label))
        return;
    if (!AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_EMAIL_ERR,
            AuthUiHelpers::ValidateEmail(ui->email_Edit->text()), ui->error_label))
        return;
    if (!AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_PWD_ERR,
            AuthUiHelpers::ValidatePassword(ui->password_Edit->text()), ui->error_label))
        return;
    if (!checkConfirmValid())
        return;
    if (!AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_VARIFY_ERR,
            AuthUiHelpers::ValidateVerifyCode(ui->verifycode_Edit->text()), ui->error_label))
        return;

    RegisterReqStruct req;
    req.user = ui->user_Edit->text();
    req.email = ui->email_Edit->text();
    // 密码使用 SHA-256 哈希后发送
    req.passwd = Utils::hashPassword(ui->password_Edit->text());
    req.varifycode = ui->verifycode_Edit->text();

    TcpMgr::Instance()->slot_send_register_req(req);
}

/**
 * @brief 获取验证码按钮点击处理
 */
void RegisterDialog::on_confirm_verifycode_Button_clicked()
{
    if (!AuthUiHelpers::ApplyValidationResult(
            _tip_errs, TipErr::TIP_EMAIL_ERR,
            AuthUiHelpers::ValidateEmail(ui->email_Edit->text()), ui->error_label))
        return;

    VerifyCodeReqStruct req;
    req.email = ui->email_Edit->text().trimmed();

    TcpMgr::Instance()->slot_send_verify_code_req(req);
}

/**
 * @brief 验证码响应处理
 * @param rsp 验证码响应结构体
 */
void RegisterDialog::slot_verify_code_rsp(const VerifyCodeRspStruct &rsp)
{
    if (!this->isVisible())
    {
        return;
    }

    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS))
    {
        QString errStr = tr("获取验证码失败");
        switch (static_cast<ERRORCODES>(rsp.error))
        {
            case ERRORCODES::UserExist:
                errStr = tr("用户名或邮箱已存在");
                break;
            default:
                break;
        }
        AuthUiHelpers::ShowTip(ui->error_label, errStr, false);
        return;
    }

    AuthUiHelpers::ShowTip(ui->error_label, tr("验证码: %1 (已发送到邮箱，注意查收)").arg(rsp.code), true);
    qDebug() << "Verification code sent to:" << rsp.email << "code:" << rsp.code;
    startVerifyCountdown(10);
}

/**
 * @brief 注册响应处理
 * @param rsp 注册响应结构体
 */
void RegisterDialog::slot_register_rsp(const RegisterRspStruct &rsp)
{
    if (!this->isVisible())
    {
        return;
    }

    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS))
    {
        // 错误码映射到用户可见的中文提示
        QString errStr = tr("注册失败");
        switch (static_cast<ERRORCODES>(rsp.error))
        {
            case ERRORCODES::UserExist:
                errStr = tr("用户名或邮箱已存在");
                break;
            case ERRORCODES::VarifyCodeErr:
                errStr = tr("验证码错误");
                break;
            case ERRORCODES::VarifyCodeExpired:
                errStr = tr("验证码已过期");
                break;
            default:
                errStr = tr("注册失败，未知错误");
                break;
        }
        AuthUiHelpers::ShowTip(ui->error_label, errStr, false);
        return;
    }

    AuthUiHelpers::ShowTip(ui->error_label, tr("用户注册成功"), true);
    qDebug() << "email is " << rsp.email;
    ChangeTipPage();
}

/**
 * @brief 切换到注册成功提示页并启动倒计时
 */
void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    _countdown = 5;
    auto str = tr("注册成功，%1 s后返回登录").arg(_countdown);
    ui->tip_lb->setText(str);
    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}

/**
 * @brief 启动验证码按钮倒计时
 * @param seconds 倒计时秒数
 */
void RegisterDialog::startVerifyCountdown(int seconds)
{
    ui->confirm_verifycode_Button->startCountdown(seconds);
}

bool RegisterDialog::checkConfirmValid()
{
    const QString confirmError =
        AuthUiHelpers::ValidateConfirmPassword(ui->password_Edit->text(), ui->confirm_password_Edit->text());
    const bool isEmptyConfirm = ui->confirm_password_Edit->text().isEmpty();

    if (!confirmError.isEmpty())
    {
        if (isEmptyConfirm)
        {
            AuthUiHelpers::AddTipError(_tip_errs, TipErr::TIP_CONFIRM_ERR, confirmError, ui->error_label);
            AuthUiHelpers::RemoveTipError(_tip_errs, TipErr::TIP_PWD_CONFIRM, ui->error_label);
        }
        else
        {
            AuthUiHelpers::AddTipError(_tip_errs, TipErr::TIP_PWD_CONFIRM, confirmError, ui->error_label);
            AuthUiHelpers::RemoveTipError(_tip_errs, TipErr::TIP_CONFIRM_ERR, ui->error_label);
        }
        return false;
    }

    AuthUiHelpers::RemoveTipError(_tip_errs, TipErr::TIP_CONFIRM_ERR, ui->error_label);
    AuthUiHelpers::RemoveTipError(_tip_errs, TipErr::TIP_PWD_CONFIRM, ui->error_label);
    return true;
}

/**
 * @brief 返回登录按钮点击处理
 */
void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_1);
    emit switchLogin();
}

/**
 * @brief 取消按钮点击处理
 */
void RegisterDialog::on_Cancel_Button_clicked()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_1);
    emit switchLogin();
}

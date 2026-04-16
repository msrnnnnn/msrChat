/**
 * @file    RegisterDialog.cpp
 * @brief   注册对话框实现
 * @note    此对话框依赖 main.cpp 启动时建立的全局 TCP 长连接。
 *          所有请求通过 TcpMgr 单例发送，无需单独建立连接。
 */
#include "RegisterDialog.h"
#include "Global.h"
#include "TcpMgr.h"
#include "ui_registerdialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>
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
    repolish(ui->error_label);

    connect(TcpMgr::Instance(), &TcpMgr::sig_verify_code_rsp, this, &RegisterDialog::slot_verify_code_rsp);
    connect(TcpMgr::Instance(), &TcpMgr::sig_register_rsp, this, &RegisterDialog::slot_register_rsp);

    connect(ui->user_Edit, &QLineEdit::editingFinished, this, [this]() { checkUserValid(); });
    connect(ui->email_Edit, &QLineEdit::editingFinished, this, [this]() { checkEmailValid(); });
    connect(ui->password_Edit, &QLineEdit::editingFinished, this, [this]() { checkPassValid(); });
    connect(ui->confirm_password_Edit, &QLineEdit::editingFinished, this, [this]() { checkConfirmValid(); });
    connect(ui->verifycode_Edit, &QLineEdit::editingFinished, this, [this]() { checkVarifyValid(); });

    ui->password_Edit->setEchoMode(QLineEdit::Password);
    ui->confirm_password_Edit->setEchoMode(QLineEdit::Password);

    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->confirm_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->pass_visible->setText(tr("显示"));
    ui->confirm_visible->setText(tr("显示"));
    connect(
        ui->pass_visible, &ClickedLabel::clicked, this,
        [this]()
        {
            auto state = ui->pass_visible->GetCurState();
            if (state == ClickLbState::Normal)
            {
                ui->password_Edit->setEchoMode(QLineEdit::Password);
                ui->pass_visible->setText(tr("显示"));
            }
            else
            {
                ui->password_Edit->setEchoMode(QLineEdit::Normal);
                ui->pass_visible->setText(tr("隐藏"));
            }
        });
    connect(
        ui->confirm_visible, &ClickedLabel::clicked, this,
        [this]()
        {
            auto state = ui->confirm_visible->GetCurState();
            if (state == ClickLbState::Normal)
            {
                ui->confirm_password_Edit->setEchoMode(QLineEdit::Password);
                ui->confirm_visible->setText(tr("显示"));
            }
            else
            {
                ui->confirm_password_Edit->setEchoMode(QLineEdit::Normal);
                ui->confirm_visible->setText(tr("隐藏"));
            }
        });

    ui->confirm_verifycode_Button->setAutoStart(false);

    _countdown_timer = new QTimer(this);
    _countdown = 5;
    connect(
        _countdown_timer, &QTimer::timeout, this,
        [this]()
        {
            if (_countdown <= 0)
            {
                _countdown_timer->stop();
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
    if (!checkUserValid())
    {
        return;
    }
    if (!checkEmailValid())
    {
        return;
    }
    if (!checkPassValid())
    {
        return;
    }
    if (!checkConfirmValid())
    {
        return;
    }
    if (!checkVarifyValid())
    {
        return;
    }

    RegisterReqStruct req;
    req.user = ui->user_Edit->text();
    req.email = ui->email_Edit->text();
    req.passwd = hashPassword(ui->password_Edit->text());
    req.varifycode = ui->verifycode_Edit->text();

    TcpMgr::Instance()->slot_send_register_req(req);
}

/**
 * @brief 获取验证码按钮点击处理
 */
void RegisterDialog::on_confirm_verifycode_Button_clicked()
{
    if (!checkEmailValid())
    {
        return;
    }

    VerifyCodeReqStruct req;
    req.email = ui->email_Edit->text().trimmed();

    TcpMgr::Instance()->slot_send_verify_code_req(req);
}

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
        showTip(errStr, false);
        return;
    }

    showTip(tr("验证码已发送到邮箱，注意查收"), true);
    qDebug() << "Verification code sent to:" << rsp.email;
    startVerifyCountdown(10);
}

void RegisterDialog::slot_register_rsp(const RegisterRspStruct &rsp)
{
    if (!this->isVisible())
    {
        return;
    }

    if (rsp.error != static_cast<int>(ERRORCODES::SUCCESS))
    {
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
        showTip(errStr, false);
        return;
    }

    showTip(tr("用户注册成功"), true);
    qDebug() << "email is " << rsp.email;
    ChangeTipPage();
}

void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    _countdown = 5;
    auto str = tr("注册成功，%1 s后返回登录").arg(_countdown);
    ui->tip_lb->setText(str);
    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}

void RegisterDialog::startVerifyCountdown(int seconds)
{
    ui->confirm_verifycode_Button->startCountdown(seconds);
}

void RegisterDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips, false);
}

void RegisterDialog::DelTipErr(TipErr te)
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

bool RegisterDialog::checkUserValid()
{
    if (ui->user_Edit->text().isEmpty())
    {
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegisterDialog::checkEmailValid()
{
    auto email = ui->email_Edit->text().trimmed();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();
    if (!match)
    {
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegisterDialog::checkPassValid()
{
    auto pass = ui->password_Edit->text();
    if (pass.length() < 6 || pass.length() > 15)
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(pass).hasMatch();
    if (!match)
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }
    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}

bool RegisterDialog::checkConfirmValid()
{
    auto confirm = ui->confirm_password_Edit->text();
    if (confirm.isEmpty())
    {
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("确认密码不能为空"));
        return false;
    }
    if (confirm != ui->password_Edit->text())
    {
        AddTipErr(TipErr::TIP_PWD_CONFIRM, tr("密码和确认密码不匹配"));
        return false;
    }
    DelTipErr(TipErr::TIP_CONFIRM_ERR);
    DelTipErr(TipErr::TIP_PWD_CONFIRM);
    return true;
}

bool RegisterDialog::checkVarifyValid()
{
    auto pass = ui->verifycode_Edit->text();
    if (pass.isEmpty())
    {
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

/**
 * @brief 显示提示信息
 */
void RegisterDialog::showTip(QString str, bool isCorrect)
{
    // 利用 QSS 的动态属性选择器 (Property Selector) 切换样式
    if (isCorrect)
    {
        ui->error_label->setProperty("state", "normal");
    }
    else
    {
        ui->error_label->setProperty("state", "error");
    }

    ui->error_label->setText(str);

    // 属性改变后，必须手动触发 repolish 才能让样式表重新计算
    repolish(ui->error_label);
}

void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_1);
    emit switchLogin();
}

void RegisterDialog::on_Cancel_Button_clicked()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_1);
    emit switchLogin();
}

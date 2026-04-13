/**
 * @file    resetdialog.cpp
 * @brief   重置密码对话框实现
 */

#include "resetdialog.h"
#include "tcpmgr.h"
#include "ui_resetdialog.h"
#include <QJsonDocument>
#include <QJsonObject>
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
    connect(
        TcpMgr::GetInstance(), static_cast<void (TcpMgr::*)(RequestType, QByteArray)>(&TcpMgr::sig_msg_received), this,
        &ResetDialog::slot_tcp_rsp);

    initHandlers();

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
    QJsonObject json_obj;
    json_obj["email"] = ui->email_edit->text();
    TcpMgr::GetInstance()->slot_send_data(
        RequestType::ID_GET_VARIFY_CODE, QJsonDocument(json_obj).toJson(QJsonDocument::Compact));
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

    QJsonObject json_obj;
    json_obj["user"] = ui->user_edit->text();
    json_obj["email"] = ui->email_edit->text();
    json_obj["passwd"] = xorString(ui->pwd_edit->text());
    json_obj["varifycode"] = ui->varify_edit->text();
    TcpMgr::GetInstance()->slot_send_data(
        RequestType::ID_RESET_PWD, QJsonDocument(json_obj).toJson(QJsonDocument::Compact));
}

/**
 * @brief TCP 回包处理
 * @param req_type 请求类型
 * @param data 响应数据
 */
void ResetDialog::slot_tcp_rsp(RequestType req_type, QByteArray data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        showTip(tr("JSON解析失败"), false);
        return;
    }

    auto it = _handlers.find(req_type);
    if (it == _handlers.end())
    {
        return;
    }
    it.value()(doc.object());
}

/**
 * @brief 初始化业务处理器
 */
void ResetDialog::initHandlers()
{
    _handlers.insert(
        RequestType::ID_GET_VARIFY_CODE,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                showTip(tr("获取验证码失败"), false);
                return;
            }
            showTip(tr("验证码已发送到邮箱，注意查收"), true);
            ui->varify_btn->startCountdown(10);
        });

    _handlers.insert(
        RequestType::ID_RESET_PWD,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                QString errStr = tr("重置失败");
                switch (static_cast<ERRORCODES>(error))
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
        });
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

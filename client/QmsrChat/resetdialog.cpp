/**
 * @file resetdialog.cpp
 * @brief 重置密码对话框实现
 * @author msr
 */

#include "resetdialog.h"
#include "global.h"
#include "httpmanagement.h"
#include "ui_resetdialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>

/**
 * @brief 构造函数
 * @details 初始化 UI 组件，连接信号槽，设置错误提示标签初始状态。
 */
ResetDialog::ResetDialog(QWidget *parent) : QDialog(parent), ui(new Ui::ResetDialog)
{
    ui->setupUi(this);

    // 连接输入框失去焦点时的校验信号
    connect(ui->user_Edit, &QLineEdit::editingFinished, this, [this]() { checkUserValid(); });
    connect(ui->email_Edit, &QLineEdit::editingFinished, this, [this]() { checkEmailValid(); });
    connect(ui->password_Edit, &QLineEdit::editingFinished, this, [this]() { checkPassValid(); });
    connect(ui->verifycode_Edit, &QLineEdit::editingFinished, this, [this]() { checkVarifyValid(); });

    // 初始化错误提示标签状态
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label);
    ui->error_label->clear();
}

/**
 * @brief 析构函数
 */
ResetDialog::~ResetDialog()
{
    delete ui;
}

/**
 * @brief 获取验证码按钮点击槽函数
 * @details 校验邮箱格式，若通过则发送 HTTP 请求获取验证码。
 */
void ResetDialog::on_verify_btn_clicked()
{
    qDebug() << "Get verify code clicked";
    auto email = ui->email_Edit->text();
    auto bcheck = checkEmailValid();
    if (!bcheck)
    {
        ui->verify_btn->Stop();
        return;
    }

    // 发送http请求获取验证码
    QJsonObject json_obj;
    json_obj["email"] = email;
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/get_varifycode"), json_obj, this,
        [this, email](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                showTip(tr("参数错误"), false);
                ui->verify_btn->Stop();
                return;
            }
            showTip(tr("验证码已发送到邮箱，注意查收"), true);
            qDebug() << "Verification code sent to:" << email;
        },
        [this](ERRORCODES)
        {
            showTip(tr("网络请求错误"), false);
            ui->verify_btn->Stop();
        });
}

/**
 * @brief 确认重置按钮点击槽函数
 * @details 校验所有输入项，若通过则发送 HTTP 请求重置密码。
 */
void ResetDialog::on_sure_btn_clicked()
{
    bool valid = checkUserValid();
    if (!valid)
    {
        return;
    }
    valid = checkEmailValid();
    if (!valid)
    {
        return;
    }
    valid = checkPassValid();
    if (!valid)
    {
        return;
    }
    valid = checkVarifyValid();
    if (!valid)
    {
        return;
    }

    // 发送http重置用户请求
    QJsonObject json_obj;
    json_obj["user"] = ui->user_Edit->text();
    json_obj["email"] = ui->email_Edit->text();
    json_obj["passwd"] = ui->password_Edit->text();
    json_obj["varifycode"] = ui->verifycode_Edit->text();

    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/reset_pwd"), json_obj, this,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                QString errStr = tr("未知错误");
                switch (static_cast<ERRORCODES>(error)) {
                case ERRORCODES::ERR_JSON:
                    errStr = tr("JSON解析失败");
                    break;
                case ERRORCODES::VARIFY_EXPIRED:
                    errStr = tr("验证码已过期");
                    break;
                case ERRORCODES::VARIFY_CODE_ERR:
                    errStr = tr("验证码错误");
                    break;
                case ERRORCODES::EMAIL_NOT_MATCH:
                    errStr = tr("邮箱不匹配");
                    break;
                case ERRORCODES::PASSWD_UP_FAILED:
                    errStr = tr("密码更新失败");
                    break;
                default:
                    errStr = tr("错误码: %1").arg(error);
                    break;
                }
                showTip(errStr, false);
                return;
            }
            auto email = jsonObj["email"].toString();
            showTip(tr("重置成功,点击返回登录"), true);
            qDebug() << "email is " << email;
            qDebug() << "user uuid is " << jsonObj["uuid"].toString();
            // 可以在这里直接切换回登录，或者让用户点击返回按钮
            // emit switchLogin();
        },
        [this](ERRORCODES) { showTip(tr("网络请求错误"), false); });
}

/**
 * @brief 取消按钮点击槽函数
 */
void ResetDialog::on_cancel_btn_clicked()
{
    emit switchLogin();
}

/**
 * @brief 校验用户名格式
 */
bool ResetDialog::checkUserValid()
{
    if (ui->user_Edit->text() == "")
    {
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

/**
 * @brief 校验密码格式
 */
bool ResetDialog::checkPassValid()
{
    auto pass = ui->password_Edit->text();
    if (pass.length() < 6 || pass.length() > 15)
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }
    // 创建一个正则表达式对象
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
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

/**
 * @brief 校验邮箱格式
 */
bool ResetDialog::checkEmailValid()
{
    auto email = ui->email_Edit->text();
    // 邮箱地址的正则表达式
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

/**
 * @brief 校验验证码非空
 */
bool ResetDialog::checkVarifyValid()
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
 * @brief 添加错误提示
 */
void ResetDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips, false);
}

/**
 * @brief 移除错误提示
 */
void ResetDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if (_tip_errs.empty())
    {
        ui->error_label->clear();
        return;
    }
    showTip(_tip_errs.first(), false);
}

/**
 * @brief 显示提示信息
 */
void ResetDialog::showTip(QString str, bool b_ok)
{
    if (b_ok)
    {
        ui->error_label->setProperty("state", "normal");
    }
    else
    {
        ui->error_label->setProperty("state", "err");
    }
    ui->error_label->setText(str);
    repolish(ui->error_label);
}

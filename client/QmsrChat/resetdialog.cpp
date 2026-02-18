#include "resetdialog.h"
#include "httpmanagement.h"
#include "ui_resetdialog.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ResetDialog)
{
    ui->setupUi(this);

    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label);

    connect(ui->cancel_btn, &QPushButton::clicked, this, &ResetDialog::switchLogin);
    connect(HttpManagement::getPtr(), &HttpManagement::signal_http_finish, this, &ResetDialog::slot_http_finish);

    initHandlers();
}

ResetDialog::~ResetDialog()
{
    delete ui;
}

bool ResetDialog::checkUserValid()
{
    if (ui->user_edit->text().isEmpty())
    {
        showTip(tr("用户名不能为空"), false);
        return false;
    }
    return true;
}

bool ResetDialog::checkPassValid()
{
    auto pass = ui->pwd_edit->text();
    if (pass.length() < 6 || pass.length() > 15)
    {
        showTip(tr("密码长度应为6~15"), false);
        return false;
    }
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    if (!regExp.match(pass).hasMatch())
    {
        showTip(tr("不能包含非法字符"), false);
        return false;
    }
    return true;
}

bool ResetDialog::checkEmailValid()
{
    auto email = ui->email_edit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    if (!regex.match(email).hasMatch())
    {
        showTip(tr("邮箱地址不正确"), false);
        return false;
    }
    return true;
}

bool ResetDialog::checkVarifyValid()
{
    auto pass = ui->varify_edit->text();
    if (pass.isEmpty())
    {
        showTip(tr("验证码不能为空"), false);
        return false;
    }
    return true;
}

void ResetDialog::on_varify_btn_clicked()
{
    auto bcheck = checkEmailValid();
    if (!bcheck)
    {
        return;
    }

    QJsonObject json_obj;
    json_obj["email"] = ui->email_edit->text();
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/get_varifycode"), json_obj, RequestType::ID_GET_VARIFY_CODE, Modules::RESETMOD);
}

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

    QJsonObject json_obj;
    json_obj["user"] = ui->user_edit->text();
    json_obj["email"] = ui->email_edit->text();
    json_obj["passwd"] = xorString(ui->pwd_edit->text());
    json_obj["varifycode"] = ui->varify_edit->text();
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/reset_pwd"), json_obj, RequestType::ID_RESET_PWD, Modules::RESETMOD);
}

void ResetDialog::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    if (mod != Modules::RESETMOD)
    {
        return;
    }
    if (err != ERRORCODES::SUCCESS)
    {
        showTip(tr("网络请求错误"), false);
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        showTip(tr("JSON解析失败"), false);
        return;
    }

    auto it = _handlers.find(req_type);
    if (it == _handlers.end())
    {
        return;
    }
    it.value()(jsonDoc.object());
}

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

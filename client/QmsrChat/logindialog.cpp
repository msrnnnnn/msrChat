/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 */
#include "logindialog.h"
#include "global.h"
#include "httpmanagement.h"
#include "tcpmgr.h"
#include "ui_logindialog.h"
#include <QJsonDocument>
#include <QJsonObject>

/**
 * @brief 构造函数
 * @details 初始化 UI 组件并连接信号槽。
 */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label);

    connect(ui->login_Button, &QPushButton::clicked, this, &LoginDialog::on_login_Button_clicked);
    connect(ui->sign_up_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    connect(HttpManagement::getPtr(), &HttpManagement::signal_http_finish, this, &LoginDialog::slot_http_finish);

    connect(this, &LoginDialog::sig_connect_tcp, TcpMgr::GetInstance().get(), &TcpMgr::slot_tcp_connect);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);

    ui->forget_password_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_password_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_password_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    ui->password_Edit->setEchoMode(QLineEdit::Password);
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->pass_visible->setText(tr("显示"));
    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]()
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

    initHandlers();
}

/**
 * @brief 析构函数
 */
LoginDialog::~LoginDialog()
{
    delete ui;
}

bool LoginDialog::checkUserValid()
{
    auto user = ui->user_Edit->text();
    if (user.isEmpty())
    {
        qDebug() << "User empty ";
        return false;
    }
    return true;
}

bool LoginDialog::checkPwdValid()
{
    auto pwd = ui->password_Edit->text();
    if (pwd.length() < 6 || pwd.length() > 15)
    {
        qDebug() << "Pass length invalid";
        return false;
    }
    return true;
}

void LoginDialog::on_login_Button_clicked()
{
    if (checkUserValid() == false)
    {
        showTip(tr("用户名不能为空"), false);
        return;
    }
    if (checkPwdValid() == false)
    {
        showTip(tr("密码长度应为6~15"), false);
        return;
    }
    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();
    // 发送http请求登录
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = xorString(pwd);
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_login"), json_obj, RequestType::ID_LOGIN_USER, Modules::LOGINMOD);
}

void LoginDialog::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    if (mod != Modules::LOGINMOD)
    {
        return;
    }
    if (err != ERRORCODES::SUCCESS)
    {
        showTip(tr("网络请求错误"), false);
        return;
    }

    QJsonDocument jsonDocument = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDocument.isNull() || !jsonDocument.isObject())
    {
        showTip(tr("JSON解析失败"), false);
        return;
    }

    auto it = _handlers.find(req_type);
    if (it == _handlers.end())
    {
        return;
    }
    it.value()(jsonDocument.object());
}

void LoginDialog::slot_forget_pwd()
{
    emit switchReset();
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if (bsuccess)
    {
        showTip(tr("聊天服务连接成功，正在登录..."), true);
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        QJsonDocument doc(jsonObj);
        QString jsonString = doc.toJson(QJsonDocument::Compact);

        TcpMgr::GetInstance()->sig_send_data(RequestType::ID_CHAT_LOGIN, jsonString);
        return;
    }

    showTip(tr("聊天服务未启动或不可用"), false);
}

void LoginDialog::initHandlers()
{
    _handlers.insert(
        RequestType::ID_LOGIN_USER,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                QString errStr = tr("登录失败");
                switch (static_cast<ERRORCODES>(error))
                {
                case ERRORCODES::PasswdErr:
                    errStr = tr("密码错误");
                    break;
                case ERRORCODES::UserNotExist:
                    errStr = tr("用户不存在");
                    break;
                case ERRORCODES::RPCGetFailed:
                    errStr = tr("状态服务不可用");
                    break;
                default:
                    break;
                }
                showTip(errStr, false);
                return;
            }

            ServerInfo si;
            si.Uid = jsonObj["uid"].toInt();
            si.Host = jsonObj["host"].toString();
            si.Port = jsonObj["port"].toString();
            si.Token = jsonObj["token"].toString();

            _uid = si.Uid;
            _token = si.Token;
            showTip(tr("登录成功，连接聊天服务..."), true);
            emit sig_connect_tcp(si);
        });
}

void LoginDialog::showTip(QString str, bool isCorrect)
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

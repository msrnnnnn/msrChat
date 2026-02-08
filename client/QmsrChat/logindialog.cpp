/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 * @author msr
 */

#include "logindialog.h"
#include "clickedlabel.h"
#include "httpmanagement.h"
#include "tcpmgr.h"
#include "ui_logindialog.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

/**
 * @brief 构造函数
 * @details 初始化 UI 组件并连接信号槽。
 */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    connect(ui->sign_up_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);

    // 初始化忘记密码标签状态
    ui->forget_password_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_password_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_password_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    // 初始化密码可见性标签
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    connect(
        ui->pass_visible, &ClickedLabel::clicked, this,
        [this]()
        {
            auto state = ui->pass_visible->GetCurState();
            if (state == ClickLbState::Normal)
            {
                ui->password_Edit->setEchoMode(QLineEdit::Password);
            }
            else
            {
                ui->password_Edit->setEchoMode(QLineEdit::Normal);
            }
            qDebug() << "Label state changed to " << (int)state;
        });

    // 初始化 HTTP 处理器
    initHttpHandlers();

    // 连接登录回包信号
    connect(
        HttpManagement::GetInstance().get(), &HttpManagement::sig_login_mod_finish, this,
        &LoginDialog::slot_login_mod_finish);

    // 连接tcp连接请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_tcp, TcpMgr::GetInstance().get(), &TcpMgr::slot_tcp_connect);
    // 连接tcp管理者发出的连接成功信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    // 连接tcp管理者发出的登陆失败信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_login_failed, this, &LoginDialog::slot_login_failed);

    // 连接登录按钮
    connect(ui->login_Button, &QPushButton::clicked, this, &LoginDialog::slot_login_btn_clicked);
}

/**
 * @brief 析构函数
 */
LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::slot_forget_pwd()
{
    qDebug() << "slot forget pwd";
    emit switchReset();
}

void LoginDialog::slot_login_btn_clicked()
{
    qDebug() << "login btn clicked";
    if (!checkUserValid())
    {
        return;
    }
    if (!checkPwdValid())
    {
        return;
    }

    enableBtn(false); // 禁用按钮防止重复点击

    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();

    // 发送 http 请求登录
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = pwd;
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_login"), json_obj, ReqId::ID_USER_LOGIN, Modules::LOGIN_MOD);
}

void LoginDialog::initHttpHandlers()
{
    // 注册获取登录回包逻辑
    _handlers.insert(
        ReqId::ID_USER_LOGIN,
        [this](QJsonObject jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ErrorCodes::SUCCESS))
            {
                showTip(tr("参数错误"), false);
                enableBtn(true);
                return;
            }
            auto user = jsonObj["user"].toString();
            // 发送信号通知tcpMgr发送长链接
            ServerInfo si;
            si.Uid = QString::number(jsonObj["uid"].toInt());
            si.Host = jsonObj["host"].toString();
            si.Port = jsonObj["port"].toString();
            si.Token = jsonObj["token"].toString();
            _uid = jsonObj["uid"].toInt();
            _token = si.Token;
            qDebug() << "user is " << user << " uid is " << si.Uid << " host is " << si.Host << " Port is " << si.Port
                     << " Token is " << si.Token;
            emit sig_connect_tcp(si);
        });
}

void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS)
    {
        showTip(tr("网络请求错误"), false);
        enableBtn(true); // 恢复按钮
        return;
    }
    // 解析 JSON 字符串,res需转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    // json解析错误
    if (jsonDoc.isNull())
    {
        showTip(tr("json解析错误"), false);
        enableBtn(true); // 恢复按钮
        return;
    }
    // json解析错误
    if (!jsonDoc.isObject())
    {
        showTip(tr("json解析错误"), false);
        enableBtn(true); // 恢复按钮
        return;
    }
    // 调用对应的逻辑,根据id回调。
    _handlers[id](jsonDoc.object());
    return;
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
        QString jsonString = doc.toJson(QJsonDocument::Indented);
        // 发送tcp请求给chat server
        emit TcpMgr::GetInstance() -> sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);
    }
    else
    {
        showTip(tr("网络异常"), false);
        enableBtn(true);
    }
}

void LoginDialog::enableBtn(bool enabled)
{
    ui->login_Button->setEnabled(enabled);
}

bool LoginDialog::checkUserValid()
{
    auto user = ui->user_Edit->text();
    if (user.isEmpty())
    {
        qDebug() << "User empty ";
        showTip(tr("用户名不能为空"), false);
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
        showTip(tr("密码长度应为6-15位"), false);
        return false;
    }
    return true;
}

void LoginDialog::showTip(QString str, bool b_ok)
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

void LoginDialog::slot_login_failed(int err)
{
    QString result = QString("登录失败, err is %1").arg(err);
    if (err == 1012)
    {
        result = QString("登录失败, token无效");
    }
    else if (err == 1011)
    {
        result = QString("登录失败, 服务器数据库查询失败(UID:1011)");
    }
    showTip(result, false);
    enableBtn(true);
}

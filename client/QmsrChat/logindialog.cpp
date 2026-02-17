/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 * @author msr
 */

#include "logindialog.h"
#include "global.h"
#include "httpmanagement.h"
#include "ui_logindialog.h"

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
}

/**
 * @brief 析构函数
 */
LoginDialog::~LoginDialog()
{
    delete ui;
}

bool LoginDialog::LoginDialog::checkUserValid()
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

void LoginDialog::on_login_btn_clicked()
{
    qDebug() << "login btn clicked";
    if (checkUserValid() == false)
    {
        return;
    }
    if (checkPwdValid() == false)
    {
        return;
    }
    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();
    // 发送http请求登录
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = xorString(pwd);
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_login"), json_obj, RequestType::ID_LOGIN_USER, Modules::LOGIN_MOD);
}

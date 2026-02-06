/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 * @author msr
 */

#include "logindialog.h"
#include "clickedlabel.h"
#include "httpmanagement.h"
#include "ui_logindialog.h"
#include <QDebug>
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

    // 连接登录按钮
    connect(ui->login_Button, &QPushButton::clicked, this, &LoginDialog::on_login_Button_clicked);
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

void LoginDialog::on_login_Button_clicked()
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

    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();

    // 发送 http 请求登录
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = pwd;

    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_login"), json_obj, this,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                showTip(tr("登录失败: 参数错误"), false);
                return;
            }
            auto user = jsonObj["user"].toString();
            showTip(tr("登录成功"), true);
            qDebug() << "Login success, user is " << user;
            // TODO: 这里可以添加跳转到主聊天界面的逻辑
        },
        [this](ERRORCODES err)
        {
            showTip(tr("网络请求错误"), false);
            qDebug() << "Login failed with error code: " << (int)err;
        });
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

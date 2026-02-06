/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 * @author msr
 */

#include "logindialog.h"
#include "ui_logindialog.h"
#include "clickedlabel.h"

/**
 * @brief 构造函数
 * @details 初始化 UI 组件并连接信号槽。
 */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    connect(ui->sign_up_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);

    // 初始化忘记密码标签状态
    ui->forget_password_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_password_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_password_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);
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

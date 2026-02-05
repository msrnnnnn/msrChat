/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 * @author msr
 */

#include "logindialog.h"
#include "ui_logindialog.h"

/**
 * @brief 构造函数
 * @details 初始化 UI 组件并连接信号槽。
 */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginDialog)
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

#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include <QRegularExpression>
#include <QMessageBox>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    connect(ui->Cancel_Button, &QPushButton::clicked, this, &RegisterDialog::switchLogin);

    ui->error_label->setProperty("state","normal");
    repolish(ui->error_label);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::on_confirm_CAPTCHA_Button_clicked()
{
    //读取输入框文本并去除首尾空格（避免空格导致验证失败）
    const QString email = ui->CAPTCHA_Edit->text().trimmed();

    //定义更严谨的邮箱验证正则
    QRegularExpression emailRegex(
        R"(^[A-Z0-9_%+-]+(?:\.[A-Z0-9_%+-]+)*@[A-Z0-9.-]+\.[A-Z]{2,}$)",
        QRegularExpression::CaseInsensitiveOption//忽略字母大小写
        );

    bool match = emailRegex.match(email).hasMatch();
    if(match){
        //发送验证码
    }else{
        showTip(tr("邮箱地址不正确"),false);
    }
}

void RegisterDialog::showTip(QString str, bool isCorrect)
{
    if(isCorrect){
        ui->error_label->setProperty("state","normal");
    }else{
        ui->error_label->setProperty("state","error");
    }
    ui->error_label->setText(str);
    repolish(ui->error_label);
}


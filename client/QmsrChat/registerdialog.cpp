/**
 * @file    registerdialog.cpp
 * @brief   注册对话框实现
 * @author  msr
 */

#include "registerdialog.h"
#include "global.h"
#include "httpmanagement.h"
#include "ui_registerdialog.h"
#include <QMessageBox>
#include <QRegularExpression>

/**
 * @brief 构造函数
 * @details 初始化 UI、连接信号槽、初始化 HTTP 处理器。
 */
RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    // 信号槽绑定：点击取消按钮切换回登录页
    connect(ui->Cancel_Button, &QPushButton::clicked, this, &RegisterDialog::switchLogin);

    // 初始化错误提示标签状态 (基于 QSS 动态属性)
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label); // 强制刷新样式，确保 state 属性生效
}

/**
 * @brief 析构函数
 */
RegisterDialog::~RegisterDialog()
{
    delete ui;
}

/**
 * @brief 确认注册按钮点击处理
 */
void RegisterDialog::on_Confirm_Button_clicked()
{
    if (ui->user_Edit->text().isEmpty())
    {
        showTip(tr("用户名不能为空"), false);
        return;
    }
    if (ui->email_Edit->text().isEmpty())
    {
        showTip(tr("邮箱不能为空"), false);
        return;
    }
    if (ui->password_Edit->text().isEmpty())
    {
        showTip(tr("密码不能为空"), false);
        return;
    }
    if (ui->confirm_password_Edit->text().isEmpty())
    {
        showTip(tr("确认密码不能为空"), false);
        return;
    }
    if (ui->confirm_password_Edit->text() != ui->password_Edit->text())
    {
        showTip(tr("密码和确认密码不匹配"), false);
        return;
    }
    if (ui->verifycode_Edit->text().isEmpty())
    {
        showTip(tr("验证码不能为空"), false);
        return;
    }

    // day11 发送http请求注册用户
    QJsonObject json_obj;
    json_obj["user"] = ui->user_Edit->text();
    json_obj["email"] = ui->email_Edit->text();
    json_obj["passwd"] = ui->password_Edit->text();
    json_obj["confirm"] = ui->confirm_password_Edit->text();
    json_obj["varifycode"] = ui->verifycode_Edit->text();

    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_register"), json_obj,
        this, // 生命周期绑定
        [this](const QJsonObject &jsonObj)
        {
            // 成功回调
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                showTip(tr("注册失败: 参数错误"), false);
                return;
            }
            showTip(tr("用户注册成功"), true);
            qDebug() << "User registered successfully.";
            // 可以选择切换到登录页
            // emit switchLogin();
        },
        [this](ERRORCODES err)
        {
            // 失败回调
            showTip(tr("注册请求失败，请检查网络"), false);
        });
}

/**
 * @brief 获取验证码按钮点击处理
 */
void RegisterDialog::on_confirm_verifycode_Button_clicked()
{
    // 1. 获取输入并清洗数据
    // trimmed() 去除首尾空格，防止用户不小心复制了空格导致校验失败
    const QString email = ui->email_Edit->text().trimmed();

    // 2. 邮箱格式校验 (工业级正则)
    // 规则：支持字母数字、常见符号，域名至少包含一个点号
    static const QRegularExpression emailRegex(
        R"(^[A-Z0-9_%+-]+(?:\.[A-Z0-9_%+-]+)*@[A-Z0-9.-]+\.[A-Z]{2,}$)",
        QRegularExpression::CaseInsensitiveOption // 忽略大小写 (User@Example.com 等同 user@example.com)
    );

    QRegularExpressionMatch resultObject = emailRegex.match(email); // 拿到报告
    bool isMatch = resultObject.hasMatch();

    if (isMatch)
    {
        QJsonObject Json_object;
        Json_object["email"] = email;
        HttpManagement::GetInstance()->PostHttpRequest(
            QUrl(gate_url_prefix + "/get_varifycode"), Json_object, this,
            [this, email](const QJsonObject &jsonObj)
            {
                int error = jsonObj["error"].toInt();
                if (error != static_cast<int>(ERRORCODES::SUCCESS))
                {
                    showTip("参数错误", false);
                    return;
                }
                showTip("验证码已发送到邮箱，注意查收", true);
                qDebug() << "Verification code sent to:" << email;
            },
            [this](ERRORCODES) { showTip("获取验证码失败", false); });
    }
    else
    {
        showTip(tr("邮箱地址不正确"), false);
    }
}

// 移除废弃的 slot_http_finish 和 initHttpHandlers

/**
 * @brief 显示提示信息
 */
void RegisterDialog::showTip(QString str, bool isCorrect)
{
    // 利用 QSS 的动态属性选择器 (Property Selector) 切换样式
    if (isCorrect)
    {
        ui->error_label->setProperty("state", "normal");
    }
    else
    {
        ui->error_label->setProperty("state", "error");
    }

    ui->error_label->setText(str);

    // [Qt 机制] 属性改变后，必须手动触发 repolish 才能让样式表重新计算
    repolish(ui->error_label);
}

/**
 * @file    registerdialog.cpp
 * @brief   注册对话框实现
 * @author  msr
 */

#include "registerdialog.h"
#include "clickedlabel.h"
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
    // connect(ui->Cancel_Button, &QPushButton::clicked, this, &RegisterDialog::switchLogin);

    _countdown = 5;

    // 初始化错误提示标签状态 (基于 QSS 动态属性)
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label); // 强制刷新样式，确保 state 属性生效

    ui->error_label->clear();

    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);

    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->confirm_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");

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
            qDebug() << "Label was clicked!";
        });

    connect(
        ui->confirm_visible, &ClickedLabel::clicked, this,
        [this]()
        {
            auto state = ui->confirm_visible->GetCurState();
            if (state == ClickLbState::Normal)
            {
                ui->confirm_password_Edit->setEchoMode(QLineEdit::Password);
            }
            else
            {
                ui->confirm_password_Edit->setEchoMode(QLineEdit::Normal);
            }
            qDebug() << "Label was clicked!";
        });

    connect(ui->user_Edit, &QLineEdit::editingFinished, this, [this]() { checkUserValid(); });
    connect(ui->email_Edit, &QLineEdit::editingFinished, this, [this]() { checkEmailValid(); });
    connect(ui->password_Edit, &QLineEdit::editingFinished, this, [this]() { checkPassValid(); });
    connect(ui->confirm_password_Edit, &QLineEdit::editingFinished, this, [this]() { checkConfirmValid(); });
    connect(ui->verifycode_Edit, &QLineEdit::editingFinished, this, [this]() { checkVarifyValid(); });

    // 创建定时器
    _countdown_timer = new QTimer(this);
    // 连接信号和槽
    connect(
        _countdown_timer, &QTimer::timeout,
        [this]()
        {
            if (_countdown == 0)
            {
                _countdown_timer->stop();
                emit switchLogin();
                return;
            }
            _countdown--;
            auto str = QString("注册成功，%1 s后返回登录").arg(_countdown);
            ui->tip_lb->setText(str);
        });
}

/**
 * @brief 析构函数
 */
RegisterDialog::~RegisterDialog()
{
    delete ui;
}

/**
 * @brief 切换到提示页面
 */
void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_2);

    // 启动定时器，设置间隔为1000毫秒（1秒）
    _countdown_timer->start(1000);
}

/**
 * @brief 确认注册按钮点击处理
 */
void RegisterDialog::on_Confirm_Button_clicked()
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
    valid = checkConfirmValid();
    if (!valid)
    {
        return;
    }
    valid = checkVarifyValid();
    if (!valid)
    {
        return;
    }

    // 发送注册请求
    QJsonObject json_obj;
    json_obj["user"] = ui->user_Edit->text();
    json_obj["email"] = ui->email_Edit->text();
    json_obj["passwd"] = ui->password_Edit->text();
    json_obj["confirm"] = ui->confirm_password_Edit->text();
    json_obj["varifycode"] = ui->verifycode_Edit->text();

    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_register"), json_obj, this,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                showTip(tr("参数错误"), false);
                return;
            }
            auto email = jsonObj["email"].toString();
            showTip(tr("用户注册成功"), true);
            qDebug() << "email is " << email;
            qDebug() << "user uuid is " << jsonObj["uuid"].toString();
            ChangeTipPage();
        });
}

void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    emit switchLogin();
}

void RegisterDialog::on_Cancel_Button_clicked()
{
    _countdown_timer->stop();
    emit switchLogin();
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

void RegisterDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips, false);
}

void RegisterDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if (_tip_errs.empty())
    {
        ui->error_label->clear();
        return;
    }
    showTip(_tip_errs.first(), false);
}

bool RegisterDialog::checkUserValid()
{
    if (ui->user_Edit->text() == "")
    {
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegisterDialog::checkPassValid()
{
    auto pass = ui->password_Edit->text();
    if (pass.length() < 6 || pass.length() > 15)
    {
        // 提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }
    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(pass).hasMatch();
    if (!match)
    {
        // 提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }
    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}

bool RegisterDialog::checkEmailValid()
{
    // 验证邮箱的地址正则表达式
    auto email = ui->email_Edit->text();
    // 邮箱地址的正则表达式
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch(); // 执行正则表达式匹配
    if (!match)
    {
        // 提示邮箱不正确
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegisterDialog::checkVarifyValid()
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

bool RegisterDialog::checkConfirmValid()
{
    auto pass = ui->password_Edit->text();
    auto confirm = ui->confirm_password_Edit->text();

    if (confirm.length() < 6 || confirm.length() > 15)
    {
        // 提示长度不准确
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("密码长度应为6~15"));
        return false;
    }
    if (confirm != pass)
    {
        // 提示密码不匹配
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("密码和确认密码不匹配"));
        return false;
    }
    DelTipErr(TipErr::TIP_CONFIRM_ERR);
    return true;
}

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

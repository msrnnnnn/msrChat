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
#include <QTimer>

/**
 * @brief 构造函数
 * @details 初始化 UI、连接信号槽、初始化 HTTP 处理器。
 */
RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    // 初始化错误提示标签状态 (基于 QSS 动态属性)
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label); // 强制刷新样式，确保 state 属性生效

    // [核心] 连接全局网络单例的信号
    // 当 HttpManagement 收到网络回包时，会触发此信号，进而调用本类的分发槽函数
    connect(HttpManagement::getPtr(), &HttpManagement::signal_http_finish, this, &RegisterDialog::slot_http_finish);

    // 初始化注册表 (填充 _handlers)
    initHttpHandlers();

    connect(ui->user_Edit, &QLineEdit::editingFinished, this, [this]()
    {
        checkUserValid();
    });
    connect(ui->email_Edit, &QLineEdit::editingFinished, this, [this]()
    {
        checkEmailValid();
    });
    connect(ui->password_Edit, &QLineEdit::editingFinished, this, [this]()
    {
        checkPassValid();
    });
    connect(ui->confirm_password_Edit, &QLineEdit::editingFinished, this, [this]()
    {
        checkConfirmValid();
    });
    connect(ui->verifycode_Edit, &QLineEdit::editingFinished, this, [this]()
    {
        checkVarifyValid();
    });

    ui->password_Edit->setEchoMode(QLineEdit::Password);
    ui->confirm_password_Edit->setEchoMode(QLineEdit::Password);

    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->confirm_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->pass_visible->setText(tr("显示"));
    ui->confirm_visible->setText(tr("显示"));
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
    connect(ui->confirm_visible, &ClickedLabel::clicked, this, [this]()
    {
        auto state = ui->confirm_visible->GetCurState();
        if (state == ClickLbState::Normal)
        {
            ui->confirm_password_Edit->setEchoMode(QLineEdit::Password);
            ui->confirm_visible->setText(tr("显示"));
        }
        else
        {
            ui->confirm_password_Edit->setEchoMode(QLineEdit::Normal);
            ui->confirm_visible->setText(tr("隐藏"));
        }
    });

    ui->confirm_verifycode_Button->setAutoStart(false);

    _countdown_timer = new QTimer(this);
    _countdown = 5;
    connect(_countdown_timer, &QTimer::timeout, this, [this]()
    {
        if (_countdown <= 0)
        {
            _countdown_timer->stop();
            ui->stackedWidget->setCurrentWidget(ui->page_1);
            emit switchLogin();
            return;
        }
        _countdown -= 1;
        auto str = tr("注册成功，%1 s后返回登录").arg(_countdown);
        ui->tip_lb->setText(str);
    });
    ui->stackedWidget->setCurrentWidget(ui->page_1);
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
    if (!checkUserValid())
    {
        return;
    }
    if (!checkEmailValid())
    {
        return;
    }
    if (!checkPassValid())
    {
        return;
    }
    if (!checkConfirmValid())
    {
        return;
    }
    if (!checkVarifyValid())
    {
        return;
    }

    // day11 发送http请求注册用户
    QJsonObject json_obj;
    json_obj["user"] = ui->user_Edit->text();
    json_obj["email"] = ui->email_Edit->text();
    json_obj["passwd"] = xorString(ui->password_Edit->text());
    json_obj["confirm"] = xorString(ui->confirm_password_Edit->text());
    json_obj["varifycode"] = ui->verifycode_Edit->text();

    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_register"), json_obj, RequestType::ID_REGISTER_USER, Modules::REGISTER_MOD);
}

/**
 * @brief 获取验证码按钮点击处理
 */
void RegisterDialog::on_confirm_verifycode_Button_clicked()
{
    if (!checkEmailValid())
    {
        return;
    }
    const QString email = ui->email_Edit->text().trimmed();
    QJsonObject Json_object;
    Json_object["email"] = email;
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/get_varifycode"), Json_object, RequestType::ID_GET_VARIFY_CODE,
        Modules::REGISTER_MOD);
}

/**
 * @brief HTTP 完成回调分发
 */
void RegisterDialog::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    if (mod != Modules::REGISTER_MOD)
    {
        return;
    }
    // 1. 网络层错误拦截
    if (err != ERRORCODES::SUCCESS)
    {
        showTip(tr("网络请求错误"), false);
        return;
    }

    // 2. JSON 解析与校验
    // res.toUtf8() 防止不同平台编码问题
    QJsonDocument jsonDocument = QJsonDocument::fromJson(res.toUtf8());

    // 校验 JSON 格式是否合法
    if (jsonDocument.isNull())
    {
        showTip("JSON解析失败", false);
        return;
    }
    // 校验最外层是否为 Object (防止数组或其他类型导致的逻辑错误)
    if (!jsonDocument.isObject())
    {
        showTip("JSON解析失败", false);
        return;
    }

    // 3. 业务逻辑分发 (Registry Pattern)
    // [Safety Fix] 绝对禁止使用 _handlers[req_type] 直接调用！
    // 原因：如果 req_type 不在 map 中，operator[] 会自动插入一个空对象并返回，
    // 导致调用空 function 抛出 std::bad_function_call 异常，程序直接 Crash。
    auto it = _handlers.find(req_type);
    if (it == _handlers.end())
    {
        // 收到未注册的回包，通常忽略或打印警告
        qWarning() << "Received unhandled request type:" << static_cast<int>(req_type);
        return;
    }

    // 安全调用对应的 Lambda
    it.value()(jsonDocument.object());
}

/**
 * @brief 初始化 HTTP 处理器
 */
void RegisterDialog::initHttpHandlers()
{
    // [注册] 获取验证码回包逻辑
    _handlers.insert(
        RequestType::ID_GET_VARIFY_CODE,
        [this](const QJsonObject &JsonObject)
        {
            int error = JsonObject["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                QString errStr = tr("获取验证码失败");
                switch (static_cast<ERRORCODES>(error))
                {
                case ERRORCODES::UserExist:
                    errStr = tr("用户名或邮箱已存在");
                    break;
                default:
                    break;
                }
                showTip(errStr, false);
                return;
            }

            auto email = JsonObject["email"].toString();
            showTip(tr("验证码已发送到邮箱，注意查收"), true);
            qDebug() << "Verification code sent to:" << email;
            startVerifyCountdown(10);
        });

    // 注册注册用户回包逻辑
    _handlers.insert(
        RequestType::ID_REGISTER_USER,
        [this](QJsonObject jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                QString errStr = tr("注册失败");
                switch (static_cast<ERRORCODES>(error))
                {
                case ERRORCODES::UserExist:
                    errStr = tr("用户名或邮箱已存在");
                    break;
                case ERRORCODES::VarifyCodeErr:
                    errStr = tr("验证码错误");
                    break;
                case ERRORCODES::VarifyCodeExpired:
                    errStr = tr("验证码已过期");
                    break;
                default:
                    errStr = tr("注册失败，未知错误");
                    break;
                }
                showTip(errStr, false);
                return;
            }
            auto email = jsonObj["email"].toString();
            showTip(tr("用户注册成功"), true);
            qDebug() << "email is " << email;
            ChangeTipPage();
        });
}

void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    _countdown = 5;
    auto str = tr("注册成功，%1 s后返回登录").arg(_countdown);
    ui->tip_lb->setText(str);
    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}

void RegisterDialog::startVerifyCountdown(int seconds)
{
    ui->confirm_verifycode_Button->startCountdown(seconds);
}

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
        ui->error_label->setProperty("state", "normal");
        ui->error_label->setText("");
        repolish(ui->error_label);
        return;
    }
    showTip(_tip_errs.first(), false);
}

bool RegisterDialog::checkUserValid()
{
    if (ui->user_Edit->text().isEmpty())
    {
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegisterDialog::checkEmailValid()
{
    auto email = ui->email_Edit->text().trimmed();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();
    if (!match)
    {
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegisterDialog::checkPassValid()
{
    auto pass = ui->password_Edit->text();
    if (pass.length() < 6 || pass.length() > 15)
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(pass).hasMatch();
    if (!match)
    {
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }
    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}

bool RegisterDialog::checkConfirmValid()
{
    auto confirm = ui->confirm_password_Edit->text();
    if (confirm.isEmpty())
    {
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("确认密码不能为空"));
        return false;
    }
    if (confirm != ui->password_Edit->text())
    {
        AddTipErr(TipErr::TIP_PWD_CONFIRM, tr("密码和确认密码不匹配"));
        return false;
    }
    DelTipErr(TipErr::TIP_CONFIRM_ERR);
    DelTipErr(TipErr::TIP_PWD_CONFIRM);
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

void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_1);
    emit switchLogin();
}

void RegisterDialog::on_Cancel_Button_clicked()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_1);
    emit switchLogin();
}

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
    : QDialog(parent), ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    // 信号槽绑定：点击取消按钮切换回登录页
    connect(ui->Cancel_Button, &QPushButton::clicked, this, &RegisterDialog::switchLogin);

    // 初始化错误提示标签状态 (基于 QSS 动态属性)
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label); // 强制刷新样式，确保 state 属性生效

    // [核心] 连接全局网络单例的信号
    // 当 HttpManagement 收到网络回包时，会触发此信号，进而调用本类的分发槽函数
    connect(HttpManagement::getPtr(), &HttpManagement::signal_http_finish, this, &RegisterDialog::slot_http_finish);

    // 初始化注册表 (填充 _handlers)
    initHttpHandlers();
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
            QUrl(gate_url_prefix + "/get_varifycode"), Json_object, RequestType::ID_GET_VARIFY_CODE,
            Modules::REGISTER_MOD);
    }
    else
    {
        showTip(tr("邮箱地址不正确"), false);
    }
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
        });
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

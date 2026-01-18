#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include <QRegularExpression>
#include <QMessageBox>
#include "httpmanagement.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    // 信号槽绑定：点击取消按钮切换回登录页
    connect(ui->Cancel_Button, &QPushButton::clicked, this, &RegisterDialog::switchLogin);

    // 初始化错误提示标签状态 (基于 QSS 动态属性)
    ui->error_label->setProperty("state","normal");
    repolish(ui->error_label); // 强制刷新样式，确保 state 属性生效

    // [核心] 连接全局网络单例的信号
    // 当 HttpManagement 收到网络回包时，会触发此信号，进而调用本类的分发槽函数
    connect(HttpManagement::getPtr(), &HttpManagement::signal_http_finish, this, &RegisterDialog::slot_http_finish);

    // 初始化注册表 (填充 _handlers)
    initHttpHandlers();
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::on_confirm_CAPTCHA_Button_clicked()
{
    // 1. 获取输入并清洗数据
    // trimmed() 去除首尾空格，防止用户不小心复制了空格导致校验失败
    const QString email = ui->CAPTCHA_Edit->text().trimmed();

    // 2. 邮箱格式校验 (工业级正则)
    // 规则：支持字母数字、常见符号，域名至少包含一个点号
    QRegularExpression emailRegex(
        R"(^[A-Z0-9_%+-]+(?:\.[A-Z0-9_%+-]+)*@[A-Z0-9.-]+\.[A-Z]{2,}$)",
        QRegularExpression::CaseInsensitiveOption // 忽略大小写 (User@Example.com 等同 user@example.com)
        );

    bool match = emailRegex.match(email).hasMatch();
    if(match){
        // TODO: 这里需要调用 HttpManagement 发送验证码请求
        // 例如: HttpManagement::getPtr()->PostHttpRequest(..., RequestType::ID_GET_VARIFY_CODE, ...);
        // 目前逻辑缺失
    }else{
        showTip(tr("邮箱地址不正确"), false);
    }
}

void RegisterDialog::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    // 1. 网络层错误拦截
    if(err != ERRORCODES::SUCCESS){
        showTip(tr("网络请求错误"), false);
        return;
    }

    // 2. JSON 解析与校验
    // res.toUtf8() 防止不同平台编码问题
    QJsonDocument jsonDocument = QJsonDocument::fromJson(res.toUtf8());

    // 校验 JSON 格式是否合法
    if(jsonDocument.isNull()){
        showTip("JSON解析失败", false);
        return;
    }
    // 校验最外层是否为 Object (防止数组或其他类型导致的逻辑错误)
    if(!jsonDocument.isObject()){
        showTip("JSON解析失败", false);
        return;
    }

    // 3. 业务逻辑分发 (Registry Pattern)
    // [Safety Fix] 绝对禁止使用 _handlers[req_type] 直接调用！
    // 原因：如果 req_type 不在 map 中，operator[] 会自动插入一个空对象并返回，
    // 导致调用空 function 抛出 std::bad_function_call 异常，程序直接 Crash。

    auto it = _handlers.find(req_type);
    if(it == _handlers.end()){
        // 收到未注册的回包，通常忽略或打印警告
        qWarning() << "Received unhandled request type:" << static_cast<int>(req_type);
        return;
    }

    // 安全调用对应的 Lambda
    it.value()(jsonDocument.object());
}

void RegisterDialog::initHttpHandlers()
{
    // [注册] 获取验证码回包逻辑
    _handlers.insert(RequestType::ID_GET_VARIFY_CODE, [this](const QJsonObject& JsonObject){
        // 1. 检查业务层错误码 (区分于网络层错误)
        // 注意：使用 static_cast 转换枚举，防止编译器警告
        int error = JsonObject["error"].toInt();
        if(error != static_cast<int>(ERRORCODES::SUCCESS)){
            showTip("参数错误", false); // 建议后续根据具体 error code 显示不同提示
            return;
        }

        // 2. 业务处理成功
        auto email = JsonObject["email"].toString();
        showTip("验证码已发送到邮箱，注意查收", true);
        qDebug() << "Verification code sent to:" << email;
    });
}

void RegisterDialog::showTip(QString str, bool isCorrect)
{
    // 利用 QSS 的动态属性选择器 (Property Selector) 切换样式
    // 在 stylesheet.qss 中通常定义为:
    // QLabel[state="normal"]{ color: green; }
    // QLabel[state="error"]{ color: red; }
    if(isCorrect){
        ui->error_label->setProperty("state", "normal");
    }else{
        ui->error_label->setProperty("state", "error");
    }

    ui->error_label->setText(str);

    // [Qt 机制] 属性改变后，必须手动触发 repolish 才能让样式表重新计算
    repolish(ui->error_label);
}

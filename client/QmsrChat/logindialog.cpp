/**
 * @file logindialog.cpp
 * @brief 登录对话框实现
 */
#include "logindialog.h"
#include "global.h"
#include "httpmanagement.h"
#include "tcpmgr.h"
#include "ui_logindialog.h"
#include "usermgr.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSettings>
#include <algorithm>

/**
 * @brief 构造函数
 * @details 初始化 UI 组件并连接信号槽。
 */
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    ui->error_label->setProperty("state", "normal");
    repolish(ui->error_label);

    connect(ui->login_Button, &QPushButton::clicked, this, &LoginDialog::on_login_Button_clicked);
    connect(ui->sign_up_Button, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    connect(HttpManagement::getPtr(), &HttpManagement::signal_http_finish, this, &LoginDialog::slot_http_finish);

    connect(this, &LoginDialog::sig_connect_tcp, TcpMgr::GetInstance(), &TcpMgr::slot_tcp_connect);
    connect(TcpMgr::GetInstance(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    connect(
        TcpMgr::GetInstance(), static_cast<void (TcpMgr::*)(quint16, QByteArray)>(&TcpMgr::sig_msg_received), this,
        &LoginDialog::slot_tcp_login_rsp);

    ui->forget_password_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_password_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_password_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);

    ui->password_Edit->setEchoMode(QLineEdit::Password);
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->pass_visible->SetState("unvisible", "unvisible_hover", "", "visible", "visible_hover", "");
    ui->pass_visible->setText(tr("显示"));
    connect(
        ui->pass_visible, &ClickedLabel::clicked, this,
        [this]()
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

    // ========== Dev 模式按钮 (开发调试用) ==========
    // 创建一个"Dev 模式"按钮，点击后直接设置 UserMgr 并连接 TCP 服务器
    QPushButton *devBtn = new QPushButton(tr("开发模式"), this);
    devBtn->setFixedSize(80, 30);
    int dev_x = std::max(10, width() - devBtn->width() - 20);
    devBtn->setGeometry(dev_x, 340, devBtn->width(), devBtn->height());
    devBtn->setStyleSheet(
        "QPushButton { background-color: #FF9800; color: white; border: none; padding: 5px; }"
        "QPushButton:hover { background-color: #F57C00; }");
    devBtn->show();
    connect(
        devBtn, &QPushButton::clicked, this,
        [this]()
        {
            qDebug() << "Dev Mode activated - skipping HTTP login";

            // 设置开发用户信息
            int devUid = 1001;
            QString devToken = "dev_token";
            UserMgr::GetInstance()->SetUid(devUid);
            UserMgr::GetInstance()->SetToken(devToken);
            _uid = devUid;
            _token = devToken;
            _chat_login_ready = false;

            showTip(tr("开发模式：直接连接 TCP..."), true);

            ServerInfo si;
            si.Uid = devUid;
            si.Token = devToken;
            QString app_path = QCoreApplication::applicationDirPath();
            QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + "config.ini");
            if (!QFile::exists(config_path))
            {
                QString current_config =
                    QDir::toNativeSeparators(QDir::currentPath() + QDir::separator() + "config.ini");
                if (QFile::exists(current_config))
                {
                    config_path = current_config;
                }
            }
            QSettings settings(config_path, QSettings::IniFormat);
            si.Host = settings.value("ChatServer/host", "").toString();
            si.Port = settings.value("ChatServer/port", "").toString();
            if (si.Host.isEmpty() || si.Port.isEmpty())
            {
                showTip(tr("ChatServer 配置缺失"), false);
                return;
            }

            qDebug() << "Dev Mode: Connecting to" << si.Host << ":" << si.Port;
            emit sig_connect_tcp(si);
        });

    initHandlers();
}

/**
 * @brief 析构函数
 */
LoginDialog::~LoginDialog()
{
    delete ui;
}

bool LoginDialog::checkUserValid()
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

/**
 * @brief 登录按钮点击处理
 */
void LoginDialog::on_login_Button_clicked()
{
    if (checkUserValid() == false)
    {
        showTip(tr("用户名不能为空"), false);
        return;
    }
    if (checkPwdValid() == false)
    {
        showTip(tr("密码长度应为6~15"), false);
        return;
    }
    auto user = ui->user_Edit->text();
    auto pwd = ui->password_Edit->text();
    // 发送http请求登录
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = xorString(pwd);
    HttpManagement::GetInstance()->PostHttpRequest(
        QUrl(gate_url_prefix + "/user_login"), json_obj, RequestType::ID_LOGIN_USER, Modules::LOGINMOD);
}

/**
 * @brief HTTP 回包处理
 * @param req_type 请求类型
 * @param res 响应内容
 * @param err 错误码
 * @param mod 模块标识
 */
void LoginDialog::slot_http_finish(RequestType req_type, QString res, ERRORCODES err, Modules mod)
{
    if (mod != Modules::LOGINMOD)
    {
        return;
    }
    if (err != ERRORCODES::SUCCESS)
    {
        showTip(tr("网络请求错误"), false);
        return;
    }

    QJsonDocument jsonDocument = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDocument.isNull() || !jsonDocument.isObject())
    {
        showTip(tr("JSON解析失败"), false);
        return;
    }

    auto it = _handlers.find(req_type);
    if (it == _handlers.end())
    {
        return;
    }
    it.value()(jsonDocument.object());
}

/**
 * @brief 忘记密码点击处理
 */
void LoginDialog::slot_forget_pwd()
{
    emit switchReset();
}

/**
 * @brief TCP 连接完成回调
 * @param bsuccess 是否连接成功
 */
void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if (bsuccess)
    {
        showTip(tr("聊天服务连接成功，正在登录..."), true);
        if (_uid <= 0 || _token.isEmpty())
        {
            showTip(tr("登录信息无效"), false);
            return;
        }
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        QJsonDocument doc(jsonObj);
        QString jsonString = doc.toJson(QJsonDocument::Compact);

        TcpMgr::GetInstance()->slot_send_data(RequestType::MSG_CHAT_LOGIN, jsonString);
        return;
    }

    showTip(tr("聊天服务未启动或不可用"), false);
}

/**
 * @brief TCP 登录回包处理
 * @param msg_id 消息类型
 * @param data 消息体
 */
void LoginDialog::slot_tcp_login_rsp(quint16 msg_id, QByteArray data)
{
    if (msg_id != static_cast<quint16>(RequestType::MSG_CHAT_LOGIN))
    {
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        showTip(tr("聊天登录响应解析失败"), false);
        return;
    }
    QJsonObject obj = doc.object();
    int error = obj.value("error").toInt(1);
    QString message = obj.value("message").toString();
    if (error != 0)
    {
        if (message.isEmpty())
        {
            message = tr("聊天登录失败");
        }
        showTip(message, false);
        return;
    }
    if (!_chat_login_ready)
    {
        _chat_login_ready = true;
        emit sig_login_success();
    }
}

/**
 * @brief 初始化登录回包处理器
 */
void LoginDialog::initHandlers()
{
    _handlers.insert(
        RequestType::ID_LOGIN_USER,
        [this](const QJsonObject &jsonObj)
        {
            int error = jsonObj["error"].toInt();
            if (error != static_cast<int>(ERRORCODES::SUCCESS))
            {
                QString errStr = tr("登录失败");
                switch (static_cast<ERRORCODES>(error))
                {
                    case ERRORCODES::PasswdErr:
                        errStr = tr("密码错误");
                        break;
                    case ERRORCODES::UserNotExist:
                        errStr = tr("用户不存在");
                        break;
                    case ERRORCODES::RPCGetFailed:
                        errStr = tr("状态服务不可用");
                        break;
                    default:
                        break;
                }
                showTip(errStr, false);
                return;
            }

            ServerInfo si;
            si.Uid = jsonObj["uid"].toInt();
            si.Host = jsonObj["host"].toString();
            si.Port = jsonObj["port"].toString();
            si.Token = jsonObj["token"].toString();

            _uid = si.Uid;
            _token = si.Token;
            _chat_login_ready = false;

            // 保存用户信息到 UserMgr 单例
            UserMgr::GetInstance()->SetUid(_uid);
            UserMgr::GetInstance()->SetToken(_token);

            showTip(tr("登录成功，连接聊天服务..."), true);
            emit sig_connect_tcp(si);
        });
}

/**
 * @brief 显示提示信息
 * @param str 提示文本
 * @param isCorrect 是否为成功提示
 */
void LoginDialog::showTip(QString str, bool isCorrect)
{
    if (isCorrect)
    {
        ui->error_label->setProperty("state", "normal");
    }
    else
    {
        ui->error_label->setProperty("state", "error");
    }
    ui->error_label->setText(str);
    repolish(ui->error_label);
}

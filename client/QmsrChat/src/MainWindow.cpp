/**
 * @file    MainWindow.cpp
 * @brief   主窗口实现
 */
#include "MainWindow.h"
#include "./ui_mainwindow.h"
#include "DPIHelper.h"
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>

static constexpr int BASE_LOGIN_WIDTH = 300;
static constexpr int BASE_LOGIN_HEIGHT = 450;
static constexpr int BASE_CHAT_WIDTH = 800;
static constexpr int BASE_CHAT_HEIGHT = 600;

static constexpr int MIN_WIDTH = 280;
static constexpr int MIN_HEIGHT = 400;
static constexpr int MAX_WIDTH = 1200;
static constexpr int MAX_HEIGHT = 900;

static constexpr int MIN_CHAT_WIDTH = 600;
static constexpr int MIN_CHAT_HEIGHT = 500;

/**
 * @brief 构造函数
 * @details 初始化窗口、子对话框及信号连接。
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. 主窗口基础设置
    this->setWindowIcon(QIcon(":/image/mainwidow.ico"));

    // 使用 DPI 适配的尺寸设置
    QSize baseLoginSize = DPI.scaledSize(BASE_LOGIN_WIDTH, BASE_LOGIN_HEIGHT);
    setMinimumSize(DPI.scaled(MIN_WIDTH), DPI.scaled(MIN_HEIGHT));
    setMaximumSize(DPI.scaled(MAX_WIDTH), DPI.scaled(MAX_HEIGHT));
    resize(baseLoginSize);

    // 隐藏 MainWindow 默认的中心控件（避免显示空白）
    if (this->centralWidget())
    {
        this->centralWidget()->hide();
    }

    // 2. 初始化登录对话框
    _login_dialog = new LoginDialog(this);
    _login_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _login_dialog->resize(baseLoginSize);
    _login_dialog->move(this->pos());
    DPI.constrainSize(_login_dialog, MIN_WIDTH, MIN_HEIGHT, MAX_WIDTH, MAX_HEIGHT);
    _login_dialog->show();

    // 3. 初始化注册对话框
    _register_dialog = new RegisterDialog(this);
    _register_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _register_dialog->resize(baseLoginSize);
    _register_dialog->move(this->pos());
    DPI.constrainSize(_register_dialog, MIN_WIDTH, MIN_HEIGHT, MAX_WIDTH, MAX_HEIGHT);
    _register_dialog->hide();

    _reset_dialog = new ResetDialog(this);
    _reset_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _reset_dialog->resize(baseLoginSize);
    _reset_dialog->move(this->pos());
    DPI.constrainSize(_reset_dialog, MIN_WIDTH, MIN_HEIGHT, MAX_WIDTH, MAX_HEIGHT);
    _reset_dialog->hide();

    // 初始化聊天对话框（初始隐藏）
    _chat_dialog = nullptr;

    // 4. 绑定切换信号槽
    connect(_login_dialog, &LoginDialog::switchRegister, this, &MainWindow::slotSwitchRegister);
    connect(_register_dialog, &RegisterDialog::switchLogin, this, &MainWindow::slotSwitchLogin);
    connect(_login_dialog, &LoginDialog::switchReset, this, &MainWindow::slotSwitchReset);
    connect(_reset_dialog, &ResetDialog::switchLogin, this, &MainWindow::slotSwitchLogin);

    // 绑定登录成功信号
    connect(_login_dialog, &LoginDialog::sig_login_success, this, &MainWindow::slotLoginSuccess);
}

/**
 * @brief 析构函数
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief 切换到注册界面
 */
void MainWindow::slotSwitchRegister()
{
    _login_dialog->hide();
    _reset_dialog->hide();
    _register_dialog->show();
}

/**
 * @brief 切换回登录界面
 */
void MainWindow::slotSwitchLogin()
{
    _register_dialog->hide();
    _reset_dialog->hide();
    _login_dialog->show();
}

/**
 * @brief 切换到重置密码界面
 */
void MainWindow::slotSwitchReset()
{
    _login_dialog->hide();
    _register_dialog->hide();
    _reset_dialog->show();
}

/**
 * @brief 处理登录成功，切换到聊天界面
 */
void MainWindow::slotLoginSuccess()
{
    qDebug() << "MainWindow::slotLoginSuccess triggered";

    // 隐藏登录相关对话框
    _login_dialog->hide();
    _register_dialog->hide();
    _reset_dialog->hide();

    // 创建并显示聊天对话框
    if (_chat_dialog == nullptr)
    {
        _chat_dialog = new ChatDialog(this);
        _chat_dialog->setWindowFlags(Qt::Widget);
        setCentralWidget(_chat_dialog);
    }

    // 使用 DPI 适配的尺寸设置聊天界面
    QSize baseChatSize = DPI.scaledSize(BASE_CHAT_WIDTH, BASE_CHAT_HEIGHT);
    setMinimumSize(DPI.scaled(MIN_CHAT_WIDTH), DPI.scaled(MIN_CHAT_HEIGHT));
    setMaximumSize(DPI.scaled(MAX_WIDTH), DPI.scaled(MAX_HEIGHT));
    resize(baseChatSize);

    DPI.centerOnScreen(this);

    show();
    raise();
    activateWindow();
    _chat_dialog->show();
}

/**
 * @brief 处理窗口关闭事件
 * @param event 关闭事件
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
    QApplication::quit();
}

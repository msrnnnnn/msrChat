#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. 主窗口基础设置
    this->setWindowIcon(QIcon(":/image/mainwidow.ico"));
    setFixedSize(300, 450);
    // 隐藏MainWindow默认的中心控件（避免显示空白）
    this->centralWidget()->hide();

    // 2. 初始化登录对话框（QDialog正确用法）
    _login_dialog = new LoginDialog(this); // 设父对象，依附主窗口
    // 先设窗口标志（无边框），再调整尺寸/位置（QDialog是顶级窗口，这行生效）
    _login_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);//(自定义窗口|无边框窗口)
    _login_dialog->setFixedSize(this->size()); // 和主窗口尺寸一致
    _login_dialog->move(this->pos()); // 和主窗口位置对齐
    _login_dialog->show(); // 显示登录界面

    // 3. 初始化注册对话框（QDialog正确用法）
    _register_dialog = new RegisterDialog(this);
    _register_dialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _register_dialog->setFixedSize(this->size());
    _register_dialog->move(this->pos());
    _register_dialog->hide(); // 初始隐藏

    // 4. 绑定切换信号槽
    connect(_login_dialog, &LoginDialog::switchRegister, this, &MainWindow::slotSwitchRegister);
    connect(_register_dialog, &RegisterDialog::switchLogin, this, &MainWindow::slotSwitchLogin);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 切换到注册界面
void MainWindow::slotSwitchRegister()
{
    _login_dialog->hide(); // 隐藏登录
    _register_dialog->show(); // 显示注册
}

// 切换回登录界面（修复崩溃的核心）
void MainWindow::slotSwitchLogin()
{
    _register_dialog->hide(); // 隐藏注册
    _login_dialog->show(); // 显示登录
}

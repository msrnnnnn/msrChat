#include "chatsidebar.h"
#include "sessionitemdelegate.h"
#include <QLabel>
#include <QStyle>

ChatSideBar::ChatSideBar(QWidget *parent)
    : QWidget(parent)
{
    initUi();
}

void ChatSideBar::initUi()
{
    // 父容器样式
    this->setStyleSheet("ChatSideBar { background-color: #F0F2F5; border-right: 1px solid #E7E7E7; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 搜索栏区域
    QWidget *searchWidget = new QWidget(this);
    searchWidget->setFixedHeight(60); // 包含上下边距
    QHBoxLayout *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(10, 10, 10, 10);

    // 搜索框容器 (用于绘制圆角背景)
    QWidget *searchContainer = new QWidget(searchWidget);
    searchContainer->setStyleSheet("QWidget { background-color: #FFFFFF; border-radius: 4px; }");
    searchContainer->setFixedHeight(30);
    QHBoxLayout *containerLayout = new QHBoxLayout(searchContainer);
    containerLayout->setContentsMargins(5, 0, 5, 0);
    containerLayout->setSpacing(5);

    // 搜索图标
    QLabel *searchIcon = new QLabel(searchContainer);
    searchIcon->setPixmap(style()->standardIcon(QStyle::SP_FileDialogInfoView).pixmap(16, 16)); // 临时替代图标
    searchIcon->setFixedSize(20, 20);
    searchIcon->setAlignment(Qt::AlignCenter);

    // 输入框
    m_pSearchEdit = new QLineEdit(searchContainer);
    m_pSearchEdit->setPlaceholderText("搜索");
    m_pSearchEdit->setStyleSheet(
        "QLineEdit { border: none; background: transparent; font-size: 12px; color: #333333; }");

    containerLayout->addWidget(searchIcon);
    containerLayout->addWidget(m_pSearchEdit);

    // 快速添加按钮
    QPushButton *addBtn = new QPushButton("+", searchWidget);
    addBtn->setFixedSize(25, 25);
    addBtn->setStyleSheet(
        "QPushButton { background-color: #E5E5E5; border-radius: 4px; color: #666666; font-weight: bold; } "
        "QPushButton:hover { background-color: #D5D5D5; }");

    searchLayout->addWidget(searchContainer);
    searchLayout->addWidget(addBtn);
    mainLayout->addWidget(searchWidget);

    // 2. 会话列表
    m_pSessionList = new QListWidget(this);
    m_pSessionList->setFrameShape(QFrame::NoFrame);
    m_pSessionList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_pSessionList->setItemDelegate(new SessionItemDelegate(this));
    m_pSessionList->setObjectName("sessionList"); // for QSS

    connect(
        m_pSessionList, &QListWidget::itemClicked,
        [this](QListWidgetItem *item) { emit sessionSelected(item->data(Qt::DisplayRole).toString()); });

    mainLayout->addWidget(m_pSessionList);

    // 3. 底部功能栏
    QWidget *bottomWidget = new QWidget(this);
    bottomWidget->setFixedHeight(50);
    bottomWidget->setStyleSheet("QWidget { border-top: 1px solid #E7E7E7; }");
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(10, 5, 10, 5);
    bottomLayout->setSpacing(20);

    // 使用 Qt 内置图标作为临时资源
    m_pBtnMsg = new QPushButton(bottomWidget);
    m_pBtnMsg->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
    m_pBtnMsg->setFixedSize(30, 30);

    m_pBtnContact = new QPushButton(bottomWidget);
    m_pBtnContact->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    m_pBtnContact->setFixedSize(30, 30);

    m_pBtnSetting = new QPushButton(bottomWidget);
    m_pBtnSetting->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_pBtnSetting->setFixedSize(30, 30);

    bottomLayout->addStretch();
    bottomLayout->addWidget(m_pBtnMsg);
    bottomLayout->addWidget(m_pBtnContact);
    bottomLayout->addWidget(m_pBtnSetting);
    bottomLayout->addStretch();

    mainLayout->addWidget(bottomWidget);
}

void ChatSideBar::addSession(const QString &name, const QString &lastMsg, const QString &time, const QPixmap &avatar)
{
    QListWidgetItem *item = new QListWidgetItem(m_pSessionList);
    item->setData(Qt::DisplayRole, name);
    item->setData(Qt::UserRole + 1, lastMsg);
    item->setData(Qt::UserRole + 2, time);
    item->setData(Qt::DecorationRole, avatar);
    m_pSessionList->addItem(item);
}

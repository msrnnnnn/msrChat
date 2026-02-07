#include "chatdialog.h"
#include <QDebug>

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent)
{
    // 设置窗口属性
    setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    resize(1046, 932); // 初始尺寸

    initUi();

    // 添加测试数据
    m_pSideBar->addSession("燃烧的胸毛", "你好，很高兴认识你！", "13:54", QPixmap());
    m_pSideBar->addSession("恋恋风辰", "Hello world!", "12:30", QPixmap());
    m_pSideBar->addSession("C++ 大佬", "C++ is the best language", "昨天", QPixmap());
}

ChatDialog::~ChatDialog()
{
}

void ChatDialog::initUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 左侧侧边栏 (25%)
    m_pSideBar = new ChatSideBar(this);
    mainLayout->addWidget(m_pSideBar, 1); // Stretch factor 1

    // 2. 右侧聊天区域 (75%)
    m_pChatArea = new ChatArea(this);
    mainLayout->addWidget(m_pChatArea, 3); // Stretch factor 3
}

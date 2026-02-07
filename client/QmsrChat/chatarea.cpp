#include "chatarea.h"
#include "chatbubble.h"
#include <QScrollBar>
#include <QTimer>
#include <QStyle>
#include <QEvent>
#include <QKeyEvent>

ChatArea::ChatArea(QWidget *parent) : QWidget(parent)
{
    initUi();
}

void ChatArea::initUi()
{
    this->setStyleSheet("ChatArea { background-color: #FFFFFF; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 标题栏
    QWidget *titleWidget = new QWidget(this);
    titleWidget->setFixedHeight(50);
    titleWidget->setStyleSheet("QWidget { background-color: #F0F2F5; border-bottom: 1px solid #E7E7E7; }");
    QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(20, 0, 20, 0);
    
    m_pTitleLabel = new QLabel("燃烧的胸毛", titleWidget);
    m_pTitleLabel->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; color: #333333; }");
    m_pTitleLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addStretch();
    titleLayout->addWidget(m_pTitleLabel);
    titleLayout->addStretch();
    
    mainLayout->addWidget(titleWidget);

    // 2. 消息滚动区
    m_pMsgScrollArea = new QScrollArea(this);
    m_pMsgScrollArea->setWidgetResizable(true);
    m_pMsgScrollArea->setFrameShape(QFrame::NoFrame);
    m_pMsgScrollArea->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");
    m_pMsgScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_pMsgContainer = new QWidget();
    m_pMsgContainer->setStyleSheet("QWidget { background-color: #FFFFFF; }");
    m_pMsgLayout = new QVBoxLayout(m_pMsgContainer);
    m_pMsgLayout->setContentsMargins(20, 20, 20, 20);
    m_pMsgLayout->setSpacing(20);
    m_pMsgLayout->addStretch(1);

    m_pMsgScrollArea->setWidget(m_pMsgContainer);
    mainLayout->addWidget(m_pMsgScrollArea);

    // 3. 输入栏
    QWidget *inputWidget = new QWidget(this);
    inputWidget->setFixedHeight(60);
    inputWidget->setStyleSheet("QWidget { background-color: #F0F2F5; border-top: 1px solid #E7E7E7; }");
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(10, 8, 10, 8);
    inputLayout->setSpacing(10);

    // 表情按钮
    QPushButton *emojiBtn = new QPushButton(inputWidget);
    emojiBtn->setFixedSize(36, 36);
    emojiBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton)); // 临时图标
    emojiBtn->setStyleSheet("QPushButton { border-radius: 8px; } QPushButton:hover { background-color: #E5E6EB; }");

    // 附件按钮
    QPushButton *fileBtn = new QPushButton(inputWidget);
    fileBtn->setFixedSize(36, 36);
    fileBtn->setIcon(style()->standardIcon(QStyle::SP_FileIcon)); // 临时图标
    fileBtn->setStyleSheet("QPushButton { border-radius: 8px; } QPushButton:hover { background-color: #E5E6EB; }");

    // 输入框
    m_pInputEdit = new QLineEdit(inputWidget);
    m_pInputEdit->setPlaceholderText("");
    m_pInputEdit->setStyleSheet("QLineEdit { background-color: #FFFFFF; border: none; border-radius: 18px; padding: 0 12px; font-size: 14px; color: #333333; }");
    m_pInputEdit->installEventFilter(this);

    // 接收按钮 (测试用)
    m_pReceiveBtn = new QPushButton("接收", inputWidget);
    m_pReceiveBtn->setFixedSize(80, 36);
    m_pReceiveBtn->setStyleSheet("QPushButton { background-color: #FFFFFF; border: 1px solid #E5E6EB; border-radius: 18px; color: #666666; } QPushButton:hover { background-color: #F5F6F7; }");
    connect(m_pReceiveBtn, &QPushButton::clicked, this, &ChatArea::onReceiveBtnClicked);

    // 发送按钮
    m_pSendBtn = new QPushButton("发送", inputWidget);
    m_pSendBtn->setFixedSize(80, 36);
    m_pSendBtn->setStyleSheet("QPushButton { background-color: #00B578; border: none; border-radius: 18px; color: #FFFFFF; } QPushButton:hover { background-color: #00A468; }");
    connect(m_pSendBtn, &QPushButton::clicked, this, &ChatArea::onSendBtnClicked);

    inputLayout->addWidget(emojiBtn);
    inputLayout->addWidget(fileBtn);
    inputLayout->addWidget(m_pInputEdit);
    inputLayout->addWidget(m_pReceiveBtn);
    inputLayout->addWidget(m_pSendBtn);

    mainLayout->addWidget(inputWidget);

    // 滚动条信号
    connect(m_pMsgScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, 
            this, &ChatArea::scrollToBottom);
}

bool ChatArea::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pInputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
             onSendBtnClicked();
             return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChatArea::onSendBtnClicked()
{
    QString text = m_pInputEdit->text();
    if (text.isEmpty()) return;
    
    addChatBubble(true, text, "我");
    m_pInputEdit->clear();
}

void ChatArea::onReceiveBtnClicked()
{
    addChatBubble(false, "你好，很高兴认识你！", "恋恋风辰");
}

void ChatArea::addChatBubble(bool isSender, const QString &text, const QString &name)
{
    // 气泡项容器
    QWidget *itemWidget = new QWidget(m_pMsgContainer);
    QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->setSpacing(5);

    // 昵称 (仅对方显示，或者根据需求都显示)
    // 根据描述：发送者昵称（对方）位于气泡上方，右对齐。
    // 这里我们简单处理：对方显示昵称，己方不显示或也显示
    
    if (!isSender) {
         QLabel *nameLabel = new QLabel(name, itemWidget);
         nameLabel->setStyleSheet("QLabel { font-size: 12px; color: #999999; }");
         nameLabel->setAlignment(Qt::AlignLeft);
         itemLayout->addWidget(nameLabel);
    } 

    ChatBubble::UserRole role = isSender ? ChatBubble::Sender : ChatBubble::Receiver;
    ChatBubble *bubble = new ChatBubble(role, text, QPixmap(), itemWidget);
    
    itemLayout->addWidget(bubble);
    
    // 将新消息插入到弹簧之前
    m_pMsgLayout->insertWidget(m_pMsgLayout->count() - 1, itemWidget);
    scrollToBottom();
}

void ChatArea::scrollToBottom()
{
    QTimer::singleShot(10, [this](){
        QScrollBar *scrollBar = m_pMsgScrollArea->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    });
}

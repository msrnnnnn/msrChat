#include "chatdialog.h"
#include "chatbubble.h"
#include <QScrollBar>
#include <QEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QTimer>

ChatDialog::ChatDialog(QWidget *parent) :
    QDialog(parent)
{
    // 设置窗口属性
    setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    resize(1000, 700);
    
    initUi();
    
    // 添加一些测试数据
    m_pContactList->addItem("联系人 A");
    m_pContactList->addItem("联系人 B");
    m_pContactList->addItem("群聊 C");
    
    // 添加初始欢迎消息
    addChatBubble(false, "你好！欢迎使用 msrChat。");
    addChatBubble(false, "这是一个基于 Qt Widgets 的纯代码界面示例。");
}

ChatDialog::~ChatDialog()
{
}

void ChatDialog::initUi()
{
    // 主布局：水平布局，无边距
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 左侧联系人列表
    m_pContactList = new QListWidget(this);
    m_pContactList->setFixedWidth(250);
    m_pContactList->setStyleSheet("QListWidget { background-color: #2e2e2e; color: white; border: none; } "
                                  "QListWidget::item { height: 60px; padding-left: 20px; } "
                                  "QListWidget::item:selected { background-color: #3e3e3e; }");
    mainLayout->addWidget(m_pContactList, 0); // Stretch factor 0

    // 2. 右侧聊天区域容器
    QWidget *rightWidget = new QWidget(this);
    rightWidget->setStyleSheet("QWidget { background-color: #f5f5f5; }");
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    mainLayout->addWidget(rightWidget, 1); // Stretch factor 1

    // 2.1 顶部标题栏
    m_pTitleLabel = new QLabel("当前聊天对象", rightWidget);
    m_pTitleLabel->setFixedHeight(50);
    m_pTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_pTitleLabel->setStyleSheet("QLabel { background-color: #f5f5f5; border-bottom: 1px solid #e7e7e7; padding-left: 20px; font-size: 16px; font-weight: bold; }");
    rightLayout->addWidget(m_pTitleLabel);

    // 2.2 中间消息滚动区域
    m_pMsgScrollArea = new QScrollArea(rightWidget);
    m_pMsgScrollArea->setWidgetResizable(true);
    m_pMsgScrollArea->setStyleSheet("QScrollArea { border: none; background-color: #f5f5f5; }");
    
    m_pMsgContainer = new QWidget();
    m_pMsgContainer->setStyleSheet("QWidget { background-color: #f5f5f5; }");
    m_pMsgLayout = new QVBoxLayout(m_pMsgContainer);
    m_pMsgLayout->setContentsMargins(20, 20, 20, 20);
    m_pMsgLayout->setSpacing(20);
    m_pMsgLayout->addStretch(1); // 顶部弹簧，使消息从底部开始堆叠或保持紧凑
    
    m_pMsgScrollArea->setWidget(m_pMsgContainer);
    rightLayout->addWidget(m_pMsgScrollArea);

    // 2.3 底部输入区域
    QWidget *inputWidget = new QWidget(rightWidget);
    inputWidget->setFixedHeight(150);
    inputWidget->setStyleSheet("QWidget { border-top: 1px solid #e7e7e7; background-color: #f5f5f5; }");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(20, 10, 20, 10);
    inputLayout->setSpacing(10);
    
    // 输入框
    m_pInputEdit = new QTextEdit(inputWidget);
    m_pInputEdit->setFrameShape(QFrame::NoFrame); // 无边框
    m_pInputEdit->setStyleSheet("QTextEdit { background-color: #f5f5f5; font-size: 14px; }");
    m_pInputEdit->installEventFilter(this); // 安装事件过滤器监听回车
    inputLayout->addWidget(m_pInputEdit);
    
    // 发送按钮栏
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    
    m_pSendBtn = new QPushButton("发送(S)", inputWidget);
    m_pSendBtn->setFixedSize(100, 30);
    m_pSendBtn->setStyleSheet("QPushButton { background-color: #e9e9e9; border: none; color: #009900; } "
                              "QPushButton:hover { background-color: #d0d0d0; }");
    connect(m_pSendBtn, &QPushButton::clicked, this, &ChatDialog::onSendBtnClicked);
    btnLayout->addWidget(m_pSendBtn);
    
    inputLayout->addLayout(btnLayout);
    rightLayout->addWidget(inputWidget);

    // 连接滚动条信号，实现自动吸底
    connect(m_pMsgScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, 
            this, &ChatDialog::scrollToBottom);
}

bool ChatDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pInputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        // Enter 发送, Ctrl+Enter 换行
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && 
            !(keyEvent->modifiers() & Qt::ControlModifier)) {
            onSendBtnClicked();
            return true; // 消费事件，不让 QTextEdit 处理（避免换行）
        }
        // Ctrl+Enter 默认会换行，不需要特殊处理
    }
    return QDialog::eventFilter(watched, event);
}

void ChatDialog::onSendBtnClicked()
{
    QString text = m_pInputEdit->toPlainText();
    if (text.isEmpty()) return;
    
    // 添加发送者消息
    addChatBubble(true, text);
    
    // 清空输入框
    m_pInputEdit->clear();
    
    // 模拟自动回复
    // QTimer::singleShot(1000, [this](){
    //    addChatBubble(false, "收到消息了！");
    // });
}

void ChatDialog::addChatBubble(bool isSender, const QString &text)
{
    ChatBubble::UserRole role = isSender ? ChatBubble::Sender : ChatBubble::Receiver;
    // 使用空 Pixmap 触发默认头像逻辑
    ChatBubble *bubble = new ChatBubble(role, text, QPixmap()); 
    
    // 将新消息添加到弹簧之前
    // 我们的布局里最后有一个 addStretch(1)，所以要 insert 到 count()-1 的位置
    // 或者简单点，我们不加弹簧，直接 addWidget，然后依靠 ScrollArea 滚动
    // 这里为了简单，我们把弹簧去掉，或者 insertWidget
    
    // 修正：m_pMsgLayout->addStretch(1) 在 initUi 里添加了
    // 插入到倒数第二个位置（弹簧之前）
    m_pMsgLayout->insertWidget(m_pMsgLayout->count() - 1, bubble);
    
    // 强制刷新滚动
    scrollToBottom();
}

void ChatDialog::scrollToBottom()
{
    // 使用 QTimer 确保在布局更新完成后执行滚动
    QTimer::singleShot(10, [this](){
        QScrollBar *scrollBar = m_pMsgScrollArea->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    });
}

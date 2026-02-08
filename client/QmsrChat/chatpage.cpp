#include "chatpage.h"
#include "chatitembase.h"
#include "chatview.h"
#include "picturebubble.h"
#include "textbubble.h"
#include "ui_chatpage.h"
#include <QPainter>
#include <QStyleOption>

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    // 设置按钮样式
    ui->receive_btn->SetState("normal", "hover", "press");
    ui->send_btn->SetState("normal", "hover", "press");

    // 设置图标样式
    ui->tool_emoji_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->tool_file_lb->SetState("normal", "hover", "press", "normal", "hover", "press");

    connect(ui->send_btn, &QPushButton::clicked, this, &ChatPage::on_send_btn_clicked);
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatPage::on_send_btn_clicked()
{
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;
    QString userName = QStringLiteral("恋恋风辰");
    QString userIcon = ":/res/head_1.jpg";

    // 这里简化处理，直接获取纯文本，实际应从 QTextEdit 解析图文混排
    // 原文档提到 pTextEdit->getMsgList()，但 customizeedit/qtextedit 默认没有这个方法
    // 我们先只支持发送纯文本

    QString content = pTextEdit->toPlainText();
    if (content.isEmpty())
    {
        return;
    }

    ChatItemBase *pChatItem = new ChatItemBase(role);
    pChatItem->setUserName(userName);
    pChatItem->setUserIcon(QPixmap(userIcon));

    QWidget *pBubble = new TextBubble(role, content);
    pChatItem->setWidget(pBubble);
    ui->chat_data_list->appendChatItem(pChatItem);

    pTextEdit->clear();
}

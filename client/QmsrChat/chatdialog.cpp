#include "chatdialog.h"
#include "ui_chatdialog.h"
#include <QAction>

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ChatDialog),
      _mode(ChatUIMode::ChatMode),
      _state(ChatUIMode::ChatMode),
      _b_loading(false)
{
    ui->setupUi(this);
    setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

    ui->add_btn->SetState("normal", "hover", "press");

    QAction *searchAction = new QAction(ui->search_edit);
    searchAction->setIcon(QIcon(":/res/search.png"));
    ui->search_edit->addAction(searchAction, QLineEdit::LeadingPosition);
    ui->search_edit->setPlaceholderText(QStringLiteral("搜索"));

    // 创建一个清除动作并设置图标
    QAction *clearAction = new QAction(ui->search_edit);
    clearAction->setIcon(QIcon(":/res/close_transparent.png"));
    // 初始时不显示清除图标
    // 将清除动作添加到LineEdit的末尾位置
    ui->search_edit->addAction(clearAction, QLineEdit::TrailingPosition);

    // 当需要显示清除图标时，更改为实际的清除图标
    connect(
        ui->search_edit, &QLineEdit::textChanged,
        [clearAction](const QString &text)
        {
            if (!text.isEmpty())
            {
                clearAction->setIcon(QIcon(":/res/close_search.png"));
            }
            else
            {
                clearAction->setIcon(QIcon(":/res/close_transparent.png")); // 文本为空时，切换回透明图标
            }
        });

    // 连接清除动作的触发信号到槽函数，用于清除文本
    connect(
        clearAction, &QAction::triggered,
        [this, clearAction]()
        {
            ui->search_edit->clear();
            clearAction->setIcon(QIcon(":/res/close_transparent.png")); // 清除文本后，切换回透明图标
            ui->search_edit->clearFocus();
            // 清除按钮被按下则不显示搜索框
            ShowSearch(false);
        });

    ui->search_edit->SetMaxLength(15);

    ShowSearch(false);
}

ChatDialog::~ChatDialog()
{
    delete ui;
}

void ChatDialog::ShowSearch(bool bsearch)
{
    if (bsearch)
    {
        ui->chat_user_list->hide();
        ui->con_user_list->hide();
        ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }
    else if (_state == ChatUIMode::ChatMode)
    {
        ui->chat_user_list->show();
        ui->con_user_list->hide();
        ui->search_list->hide();
        _mode = ChatUIMode::ChatMode;
    }
    else if (_state == ChatUIMode::ContactMode)
    {
        ui->chat_user_list->hide();
        ui->search_list->hide();
        ui->con_user_list->show();
        _mode = ChatUIMode::ContactMode;
    }
}

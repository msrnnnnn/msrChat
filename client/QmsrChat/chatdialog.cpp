#include "chatdialog.h"
#include "chatuserwid.h"
#include "ui_chatdialog.h"
#include <QAction>
#include <QRandomGenerator>

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

    connect(ui->search_edit, &QLineEdit::textChanged, this, &ChatDialog::on_search_edit_textChanged);

    ShowSearch(false);
    addChatUserList();
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

void ChatDialog::addChatUserList()
{
    // 创建QListWidgetItem，并设置自定义的widget
    _msgs = {
        "hello world !", "nice to meet u", "New year，new life", "You have to love yourself",
        "My love is written in the wind ever since the whole world is you"};

    _heads = {":/res/head_1.jpg", ":/res/head_2.jpg", ":/res/head_3.jpg", ":/res/head_4.jpg", ":/res/head_5.jpg"};

    _names = {"llfc", "zack", "golang", "cpp", "java", "nodejs", "python", "rust"};

    loadingChatUser();
}

void ChatDialog::loadingChatUser()
{
    for (int i = 0; i < 13; i++)
    {
        int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
        int str_i = randomValue % _msgs.size();
        int head_i = randomValue % _heads.size();
        int name_i = randomValue % _names.size();

        auto *chat_user_wid = new ChatUserWid();
        chat_user_wid->SetInfo(_names[name_i], _heads[head_i], _msgs[str_i]);
        QListWidgetItem *item = new QListWidgetItem;
        // qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(chat_user_wid->sizeHint());
        ui->chat_user_list->addItem(item);
        ui->chat_user_list->setItemWidget(item, chat_user_wid);
    }
}

void ChatDialog::on_search_edit_textChanged(const QString &arg1)
{
    if (!arg1.isEmpty())
    {
        ShowSearch(true);
        handleSearch(arg1);
    }
    else
    {
        ShowSearch(false);
    }
}

void ChatDialog::handleSearch(const QString &text)
{
    if (text.isEmpty())
    {
        return;
    }

    ui->search_list->clear();

    // Simple local search implementation
    for (size_t i = 0; i < _names.size(); i++)
    {
        if (_names[i].contains(text, Qt::CaseInsensitive))
        {
            auto *chat_user_wid = new ChatUserWid();
            // Use random head/msg for demo consistency
            int randomValue = QRandomGenerator::global()->bounded(100);
            int str_i = randomValue % _msgs.size();
            int head_i = randomValue % _heads.size();

            chat_user_wid->SetInfo(_names[i], _heads[head_i], _msgs[str_i]);
            QListWidgetItem *item = new QListWidgetItem;
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->search_list->addItem(item);
            ui->search_list->setItemWidget(item, chat_user_wid);
        }
    }
}

bool ChatDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 如果有特定的事件需要处理，可以在这里添加逻辑
    // 例如处理点击外部关闭搜索框等
    // 目前默认透传给基类
    return QDialog::eventFilter(watched, event);
}

#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include "chatpage.h"
#include "global.h"
#include <QDialog>

namespace Ui
{
class ChatDialog;
}

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

    enum class ChatUIMode
    {
        ChatMode,
        ContactMode,
        SearchMode
    };

    void ShowSearch(bool bsearch = false);
    void addChatUserList();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void handleSearch(const QString &text);

private slots:
    void on_search_edit_textChanged(const QString &arg1);
    void slot_loading_chat_user();
    void slot_side_chat();
    void slot_side_contact();
    void slot_text_changed(const QString &str);
    void slotSwitchApplyFriendPage();

private:
    void loadingChatUser();
    Ui::ChatDialog *ui;
    ChatUIMode _mode;
    ChatUIMode _state;
    bool _b_loading;
    std::vector<QString> _names;
    std::vector<QString> _heads;
    std::vector<QString> _msgs;
    ChatPage *_chat_page;
};

#endif // CHATDIALOG_H

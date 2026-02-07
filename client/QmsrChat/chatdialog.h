#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include "chatarea.h"
#include "chatsidebar.h"
#include "global.h"
#include <QDialog>
#include <QHBoxLayout>

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

private:
    void initUi();

    ChatSideBar *m_pSideBar;
    ChatArea *m_pChatArea;
};

#endif // CHATDIALOG_H

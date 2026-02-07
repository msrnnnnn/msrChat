#ifndef CHATAREA_H
#define CHATAREA_H

#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ChatArea : public QWidget
{
    Q_OBJECT
public:
    explicit ChatArea(QWidget *parent = nullptr);

    void addChatBubble(bool isSender, const QString &text, const QString &name);

private slots:
    void onSendBtnClicked();
    void onReceiveBtnClicked();
    void scrollToBottom();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void initUi();

    QLabel *m_pTitleLabel;
    QScrollArea *m_pMsgScrollArea;
    QWidget *m_pMsgContainer;
    QVBoxLayout *m_pMsgLayout;
    
    QLineEdit *m_pInputEdit;
    QPushButton *m_pSendBtn;
    QPushButton *m_pReceiveBtn;
};

#endif // CHATAREA_H

#ifndef CHATSIDEBAR_H
#define CHATSIDEBAR_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ChatSideBar : public QWidget
{
    Q_OBJECT
public:
    explicit ChatSideBar(QWidget *parent = nullptr);

    void addSession(const QString &name, const QString &lastMsg, const QString &time, const QPixmap &avatar = QPixmap());

signals:
    void sessionSelected(const QString &name);

private:
    void initUi();

    QLineEdit *m_pSearchEdit;
    QListWidget *m_pSessionList;
    QPushButton *m_pBtnMsg;
    QPushButton *m_pBtnContact;
    QPushButton *m_pBtnSetting;
};

#endif // CHATSIDEBAR_H

#ifndef CONTACTUSERLIST_H
#define CONTACTUSERLIST_H

#include "conuseritem.h"
#include <QListWidget>

class ContactUserList : public QListWidget
{
    Q_OBJECT
public:
    ContactUserList(QWidget *parent = nullptr);
    void showRedPoint(bool bshow = true);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void addContactUserList();
public slots:
    void slotItemClicked(QListWidgetItem *item);
signals:
    void sigLoadingContactUser();
    void sigSwitchApplyFriendPage();
    void sigSwitchFriendInfoPage();

private:
    ConUserItem *addFriendItem_;
    QListWidgetItem *groupItem_;
};

#endif // CONTACTUSERLIST_H

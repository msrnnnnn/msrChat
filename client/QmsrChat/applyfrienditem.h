#ifndef APPLYFRIENDITEM_H
#define APPLYFRIENDITEM_H

#include "global.h"
#include "listitembase.h"
#include <QWidget>
#include <memory>

namespace Ui
{
class ApplyFriendItem;
}

class ApplyFriendItem : public ListItemBase
{
    Q_OBJECT

public:
    explicit ApplyFriendItem(QWidget *parent = nullptr);
    ~ApplyFriendItem();
    void setInfo(std::shared_ptr<SearchInfo> si, bool added);
    void showAddBtn(bool show);
    QSize sizeHint() const override;
    int getUid();

signals:
    void sigAuthFriend(std::shared_ptr<SearchInfo> si);

private slots:
    void on_addBtn_clicked();

private:
    Ui::ApplyFriendItem *ui;
    std::shared_ptr<SearchInfo> si_;
    bool added_;
};

#endif // APPLYFRIENDITEM_H

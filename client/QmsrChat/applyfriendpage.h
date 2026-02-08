#ifndef APPLYFRIENDPAGE_H
#define APPLYFRIENDPAGE_H

#include "applyfrienditem.h"
#include "global.h"
#include <QMap>
#include <QWidget>
#include <memory>

namespace Ui
{
class ApplyFriendPage;
}

class ApplyFriendPage : public QWidget
{
    Q_OBJECT

public:
    explicit ApplyFriendPage(QWidget *parent = nullptr);
    ~ApplyFriendPage();
    void addNewApply(std::shared_ptr<SearchInfo> si);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::ApplyFriendPage *ui;
    QMap<int, ApplyFriendItem *> applyItems_;

public slots:
    void slotAuthFriend(std::shared_ptr<SearchInfo> si);
signals:
    void sigShowSearch(bool);
};

#endif // APPLYFRIENDPAGE_H

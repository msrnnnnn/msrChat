#ifndef CONUSERITEM_H
#define CONUSERITEM_H

#include "listitembase.h"
#include <QWidget>

namespace Ui
{
class ConUserItem;
}

class ConUserItem : public ListItemBase
{
    Q_OBJECT
public:
    explicit ConUserItem(QWidget *parent = nullptr);
    ~ConUserItem();
    QSize sizeHint() const override;
    void setInfo(QString name, QString icon);
    void showRedPoint(bool show = false);

private:
    Ui::ConUserItem *ui;
    QString name_;
    QString icon_;
};

#endif // CONUSERITEM_H

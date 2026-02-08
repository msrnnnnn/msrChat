#ifndef CLICKEDONCELABEL_H
#define CLICKEDONCELABEL_H

#include <QLabel>
#include <QMouseEvent>

class ClickedOnceLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickedOnceLabel(QWidget *parent = nullptr);
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void clicked(QString);
};

#endif // CLICKEDONCELABEL_H

#ifndef CLICKEDLABEL_H
#define CLICKEDLABEL_H

#include "global.h"
#include <QLabel>

class ClickedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickedLabel(QWidget *parent = nullptr);

    void SetState(QString normal = "", QString hover = "", QString press = "", QString select = "",
                  QString select_hover = "", QString select_press = "");
    ClickLbState GetCurState();

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString _normal;
    QString _normal_hover;
    QString _normal_press;

    QString _selected;
    QString _selected_hover;
    QString _selected_press;

    ClickLbState _curstate;
};

#endif // CLICKEDLABEL_H

#include "clickedbtn.h"
#include <QStyle>
#include <QVariant>

ClickedBtn::ClickedBtn(QWidget *parent)
    : QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
}

ClickedBtn::~ClickedBtn()
{
}

void ClickedBtn::SetState(QString normal, QString hover, QString press)
{
    _normal = normal;
    _hover = hover;
    _press = press;
    setProperty("state", normal);
    style()->polish(this);
    update();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ClickedBtn::enterEvent(QEnterEvent *event)
#else
void ClickedBtn::enterEvent(QEvent *event)
#endif
{
    setProperty("state", _hover);
    style()->polish(this);
    update();
    QPushButton::enterEvent(event);
}

void ClickedBtn::leaveEvent(QEvent *event)
{
    setProperty("state", _normal);
    style()->polish(this);
    update();
    QPushButton::leaveEvent(event);
}

void ClickedBtn::mousePressEvent(QMouseEvent *event)
{
    setProperty("state", _press);
    style()->polish(this);
    update();
    QPushButton::mousePressEvent(event);
}

void ClickedBtn::mouseReleaseEvent(QMouseEvent *event)
{
    setProperty("state", _hover);
    style()->polish(this);
    update();
    QPushButton::mouseReleaseEvent(event);
}

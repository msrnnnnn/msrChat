/**
 * @file    clickedlabel.cpp
 * @brief   自定义可点击标签类实现
 */

#include "clickedlabel.h"
#include <QMouseEvent>

ClickedLabel::ClickedLabel(QWidget *parent)
    : QLabel(parent),
      _curstate(ClickLbState::Normal)
{
}

void ClickedLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (_curstate == ClickLbState::Normal)
        {
            _curstate = ClickLbState::Selected;
            setProperty("state", _selected_press.isEmpty() ? _selected_hover : _selected_press);
        }
        else
        {
            _curstate = ClickLbState::Normal;
            setProperty("state", _normal_press.isEmpty() ? _normal_hover : _normal_press);
        }
        repolish(this);
        update();
        emit clicked();
    }
    QLabel::mousePressEvent(event);
}

void ClickedLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (_curstate == ClickLbState::Normal)
        {
            setProperty("state", _normal_hover);
        }
        else
        {
            setProperty("state", _selected_hover);
        }
        repolish(this);
        update();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ClickedLabel::enterEvent(QEnterEvent *event)
#else
void ClickedLabel::enterEvent(QEvent *event)
#endif
{
    if (_curstate == ClickLbState::Normal)
    {
        setProperty("state", _normal_hover);
    }
    else
    {
        setProperty("state", _selected_hover);
    }
    repolish(this);
    update();
    QLabel::enterEvent(event);
}

void ClickedLabel::leaveEvent(QEvent *event)
{
    if (_curstate == ClickLbState::Normal)
    {
        setProperty("state", _normal);
    }
    else
    {
        setProperty("state", _selected);
    }
    repolish(this);
    update();
    QLabel::leaveEvent(event);
}

void ClickedLabel::SetState(QString normal, QString hover, QString press, QString select, QString select_hover,
                            QString select_press)
{
    _normal = normal;
    _normal_hover = hover;
    _normal_press = press;

    _selected = select;
    _selected_hover = select_hover;
    _selected_press = select_press;

    setProperty("state", normal);
    repolish(this);
}

ClickLbState ClickedLabel::GetCurState()
{
    return _curstate;
}

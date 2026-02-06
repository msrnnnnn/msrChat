#include "clickedlabel.h"
#include <QDebug>
#include <QMouseEvent>

ClickedLabel::ClickedLabel(QWidget *parent)
    : QLabel(parent),
      _curstate(ClickLbState::Normal)
{
    // 设置鼠标追踪，以便接收 enter/leave 事件（虽然 enter/leave 不需要 mouseTracking，但如果需要 hover 移动检测则需要）
    // 这里为了保险起见，或者如果将来需要 hover 坐标
    this->setMouseTracking(true);
}

void ClickedLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (_curstate == ClickLbState::Normal)
        {
            // 当前是 Normal，点击后变为 Selected
            // 点击的一瞬间，通常保持 hover 状态或者切换到 selected_hover
            // 逻辑：点击 -> 切换状态。如果鼠标还在上面，应该是 selected_hover
            qDebug() << "clicked , change to selected hover: " << _selected_hover;
            _curstate = ClickLbState::Selected;
            setProperty("state", _selected_hover);
        }
        else
        {
            // 当前是 Selected，点击后变为 Normal
            qDebug() << "clicked , change to normal hover: " << _normal_hover;
            _curstate = ClickLbState::Normal;
            setProperty("state", _normal_hover);
        }

        repolish(this);
        update();
        emit clicked();
    }

    // 调用基类处理
    QLabel::mousePressEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ClickedLabel::enterEvent(QEnterEvent *event)
#else
void ClickedLabel::enterEvent(QEvent *event)
#endif
{
    if (_curstate == ClickLbState::Normal)
    {
        qDebug() << "enter , change to normal hover: " << _normal_hover;
        setProperty("state", _normal_hover);
    }
    else
    {
        qDebug() << "enter , change to selected hover: " << _selected_hover;
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
        qDebug() << "leave , change to normal : " << _normal;
        setProperty("state", _normal);
    }
    else
    {
        qDebug() << "leave , change to normal hover: " << _selected;
        setProperty("state", _selected);
    }

    repolish(this);
    update();
    QLabel::leaveEvent(event);
}

void ClickedLabel::SetState(
    QString normal, QString hover, QString press, QString select, QString select_hover, QString select_press)
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

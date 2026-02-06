/**
 * @file clickedlabel.cpp
 * @brief 可点击标签控件实现
 * @author msr
 */

#include "clickedlabel.h"
#include <QDebug>
#include <QMouseEvent>

/**
 * @brief 构造函数
 * @details 初始化标签，开启鼠标追踪以支持悬浮事件。
 */
ClickedLabel::ClickedLabel(QWidget *parent)
    : QLabel(parent),
      _curstate(ClickLbState::Normal)
{
    // 设置鼠标追踪，以便接收 enter/leave 事件
    this->setMouseTracking(true);
}

/**
 * @brief 鼠标点击事件处理
 * @details 处理左键点击，切换选中状态 (Normal <-> Selected)，并更新样式属性 state。
 */
void ClickedLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (_curstate == ClickLbState::Normal)
        {
            // 当前是 Normal，点击后变为 Selected
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

/**
 * @brief 鼠标进入事件处理
 * @details 鼠标移入时，根据当前状态切换到对应的悬浮样式 (Normal Hover / Selected Hover)。
 */
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

/**
 * @brief 鼠标离开事件处理
 * @details 鼠标移出时，恢复到对应的基础状态样式 (Normal / Selected)。
 */
void ClickedLabel::leaveEvent(QEvent *event)
{
    if (_curstate == ClickLbState::Normal)
    {
        qDebug() << "leave , change to normal : " << _normal;
        setProperty("state", _normal);
    }
    else
    {
        qDebug() << "leave , change to selected : " << _selected;
        setProperty("state", _selected);
    }

    repolish(this);
    update();
    QLabel::leaveEvent(event);
}

/**
 * @brief 设置各个状态下的 QSS 属性值
 * @details 初始化控件在不同交互状态下的 QSS 属性字符串，并设置默认状态为 normal。
 */
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

/**
 * @brief 获取当前状态
 */
ClickLbState ClickedLabel::GetCurState()
{
    return _curstate;
}

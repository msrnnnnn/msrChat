/**
 * @file    ClickedLabel.cpp
 * @brief   自定义可点击标签类实现
 */

#include "ClickedLabel.h"
#include <QMouseEvent>

/**
 * @brief 构造函数
 * @param parent 父窗口
 */
ClickedLabel::ClickedLabel(QWidget *parent)
    : QLabel(parent),
      _curstate(ClickLbState::Normal)
{
}

/**
 * @brief 处理鼠标按下事件并切换状态
 * @param event 鼠标事件
 */
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

/**
 * @brief 处理鼠标释放事件并恢复悬停样式
 * @param event 鼠标事件
 */
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
/**
 * @brief 处理鼠标进入事件
 * @param event 进入事件
 */
void ClickedLabel::enterEvent(QEnterEvent *event)
#else
/**
 * @brief 处理鼠标进入事件
 * @param event 进入事件
 */
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

/**
 * @brief 处理鼠标离开事件
 * @param event 离开事件
 */
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

/**
 * @brief 设置各状态样式名
 * @param normal 普通状态
 * @param hover 悬停状态
 * @param press 按下状态
 * @param select 选中状态
 * @param select_hover 选中悬停状态
 * @param select_press 选中按下状态
 */
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

/**
 * @brief 获取当前状态
 * @return ClickLbState 当前状态
 */
ClickLbState ClickedLabel::GetCurState()
{
    return _curstate;
}

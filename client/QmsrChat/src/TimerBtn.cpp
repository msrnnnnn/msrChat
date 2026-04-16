/**
 * @file    TimerBtn.cpp
 * @brief   倒计时按钮控件实现
 */

#include "TimerBtn.h"
#include <QMouseEvent>

/**
 * @brief 构造函数
 * @param parent 父窗口
 */
TimerBtn::TimerBtn(QWidget *parent)
    : QPushButton(parent),
      _timer(new QTimer(this)),
      _counter(0),
      _total(0),
      _defaultText("获取"),
      _autoStart(true)
{
    connect(_timer, &QTimer::timeout, this, [this]()
            {
                if (_counter <= 1)
                {
                    stopCountdown();
                    return;
                }
                _counter -= 1;
                updateText();
            });
}

/**
 * @brief 析构函数
 */
TimerBtn::~TimerBtn()
{
    stopCountdown();
}

/**
 * @brief 设置是否自动启动倒计时
 * @param autoStart 是否自动启动
 */
void TimerBtn::setAutoStart(bool autoStart)
{
    _autoStart = autoStart;
}

/**
 * @brief 启动倒计时
 * @param seconds 倒计时秒数
 */
void TimerBtn::startCountdown(int seconds)
{
    if (seconds <= 0)
    {
        stopCountdown();
        return;
    }

    if (!text().isEmpty())
    {
        _defaultText = text();
    }
    _total = seconds;
    _counter = seconds;
    setEnabled(false);
    updateText();
    if (!_timer->isActive())
    {
        _timer->start(1000);
    }
}

/**
 * @brief 停止倒计时并恢复默认文本
 */
void TimerBtn::stopCountdown()
{
    if (_timer->isActive())
    {
        _timer->stop();
    }
    _counter = 0;
    _total = 0;
    setText(_defaultText.isEmpty() ? "获取" : _defaultText);
    setEnabled(true);
}

/**
 * @brief 鼠标释放事件
 * @param e 鼠标事件
 */
void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && _autoStart)
    {
        startCountdown(10);
    }
    QPushButton::mouseReleaseEvent(e);
}

/**
 * @brief 更新按钮文本显示
 */
void TimerBtn::updateText()
{
    if (_counter > 0)
    {
        setText(QString::number(_counter) + "s");
        return;
    }
    setText(_defaultText.isEmpty() ? "获取" : _defaultText);
}

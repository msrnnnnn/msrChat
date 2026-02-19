#include "timerbtn.h"
#include <QMouseEvent>

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

TimerBtn::~TimerBtn()
{
    stopCountdown();
}

void TimerBtn::setAutoStart(bool autoStart)
{
    _autoStart = autoStart;
}

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

void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && _autoStart)
    {
        startCountdown(10);
    }
    QPushButton::mouseReleaseEvent(e);
}

void TimerBtn::updateText()
{
    if (_counter > 0)
    {
        setText(QString::number(_counter) + "s");
        return;
    }
    setText(_defaultText.isEmpty() ? "获取" : _defaultText);
}

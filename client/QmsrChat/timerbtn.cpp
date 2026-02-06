#include "timerbtn.h"
#include <QDebug>
#include <QMouseEvent>

TimerBtn::TimerBtn(QWidget *parent) : QPushButton(parent), _counter(10)
{
    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, [this]() {
        _counter--;
        if (_counter <= 0)
        {
            _timer->stop();
            _counter = 10;
            this->setText("获取");
            this->setEnabled(true);
            return;
        }
        this->setText(QString::number(_counter) + " s");
    });
}

TimerBtn::~TimerBtn()
{
    _timer->stop();
}

void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        // 触发 clicked 信号，让外部处理业务逻辑（如发送验证码请求）
        // 注意：我们不在这里启动定时器，而是由外部业务逻辑决定是否启动
        // 因为如果发送验证码失败（如邮箱格式错误），不应该进入倒计时
        
        // 但根据用户提供的教程代码，它是点击后立即开始倒计时。
        // 为了更好的用户体验，建议：点击 -> 检查格式 -> 发送请求 -> (成功)开始倒计时
        // 不过为了兼容教程逻辑，这里先按教程写，后续可以优化提供一个 startTimer 接口供外部调用
        
        // 教程逻辑：点击即倒计时 (不够严谨，但先实现)
        qDebug() << "TimerBtn clicked";
        this->setEnabled(false);
        this->setText(QString::number(_counter) + " s");
        _timer->start(1000);
        
        emit clicked();
    }
    
    // 调用基类处理（如绘制按下效果）
    QPushButton::mouseReleaseEvent(e);
}

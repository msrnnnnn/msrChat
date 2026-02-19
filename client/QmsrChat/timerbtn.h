#ifndef TIMERBTN_H
#define TIMERBTN_H

#include <QPushButton>
#include <QTimer>

class TimerBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit TimerBtn(QWidget *parent = nullptr);
    ~TimerBtn();

    void startCountdown(int seconds);
    void stopCountdown();
    void setAutoStart(bool autoStart);

protected:
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void updateText();

    QTimer *_timer;
    int _counter;
    int _total;
    QString _defaultText;
    bool _autoStart;
};

#endif // TIMERBTN_H

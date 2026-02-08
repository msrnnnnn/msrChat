#ifndef CUSTOMIZEEDIT_H
#define CUSTOMIZEEDIT_H

#include <QDebug>
#include <QLineEdit>

class CustomizeEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit CustomizeEdit(QWidget *parent = nullptr);
    void SetMaxLength(int maxLen);

protected:
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    void limitTextLength(QString text);
    int _max_len;

signals:
    void sig_focus_out();
};

#endif // CUSTOMIZEEDIT_H

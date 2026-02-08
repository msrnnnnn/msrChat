#include "customizeedit.h"

CustomizeEdit::CustomizeEdit(QWidget *parent)
    : QLineEdit(parent),
      _max_len(0)
{
    connect(this, &QLineEdit::textChanged, this, &CustomizeEdit::limitTextLength);
}

void CustomizeEdit::SetMaxLength(int maxLen)
{
    _max_len = maxLen;
}

void CustomizeEdit::limitTextLength(QString text)
{
    if (_max_len <= 0)
        return;
    QByteArray byteArray = text.toUtf8();
    if (byteArray.size() > _max_len)
    {
        byteArray = byteArray.left(_max_len);
        this->setText(QString::fromUtf8(byteArray));
    }
}

void CustomizeEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
}

void CustomizeEdit::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);
    emit sig_focus_out();
}

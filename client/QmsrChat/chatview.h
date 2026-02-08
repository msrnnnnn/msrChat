#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QListWidget>

class ChatView : public QListWidget
{
    Q_OBJECT
public:
    explicit ChatView(QWidget *parent = nullptr);
};

#endif // CHATVIEW_H

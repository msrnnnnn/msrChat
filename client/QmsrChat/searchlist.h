#ifndef SEARCHLIST_H
#define SEARCHLIST_H

#include <QListWidget>
#include <QEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QDebug>

class SearchList : public QListWidget
{
    Q_OBJECT
public:
    explicit SearchList(QWidget *parent = nullptr);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
signals:
    void sig_loading_search_user();
};

#endif // SEARCHLIST_H

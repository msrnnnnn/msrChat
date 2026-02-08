#ifndef SEARCHLIST_H
#define SEARCHLIST_H

#include <QListWidget>

class SearchList : public QListWidget
{
    Q_OBJECT
public:
    explicit SearchList(QWidget *parent = nullptr);
};

#endif // SEARCHLIST_H

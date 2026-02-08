#ifndef STATELABEL_H
#define STATELABEL_H

#include "clickedlabel.h"

class StateLabel : public ClickedLabel
{
    Q_OBJECT
public:
    explicit StateLabel(QWidget *parent = nullptr);

    // Define states specific to side bar labels if needed
    // For now, it inherits ClickedLabel functionality
};

#endif // STATELABEL_H

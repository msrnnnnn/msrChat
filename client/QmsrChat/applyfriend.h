#ifndef APPLYFRIEND_H
#define APPLYFRIEND_H

#include "clickedlabel.h"
#include "clickedoncelabel.h"
#include "global.h"
#include <QDialog>
#include <QMap>
#include <memory>
#include <vector>

namespace Ui
{
class ApplyFriend;
}

class ApplyFriend : public QDialog
{
    Q_OBJECT

public:
    explicit ApplyFriend(QWidget *parent = nullptr);
    ~ApplyFriend();
    void initTipLbs();
    void addTipLbs(ClickedLabel *, QPoint cur_point, QPoint &next_point, int text_width, int text_height);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void setSearchInfo(std::shared_ptr<SearchInfo> si);

private:
    Ui::ApplyFriend *ui;
    void resetLabels();

    using FriendLabel = ClickedOnceLabel;

    QMap<QString, ClickedLabel *> addLabels_;
    std::vector<QString> addLabelKeys_;
    QPoint labelPoint_;

    QMap<QString, FriendLabel *> friendLabels_;
    std::vector<QString> friendLabelKeys_;

    void addLabel(QString name);
    std::vector<QString> tipData_;
    QPoint tipCurPoint_;
    std::shared_ptr<SearchInfo> si_;

public slots:
    void showMoreLabel();
    void slotLabelEnter();
    void slotRemoveFriendLabel(QString);
    void slotChangeFriendLabelByTip(QString, ClickLbState);
    void slotLabelTextChange(const QString &text);
    void slotLabelEditFinished();
    void slotAddFirendLabelByClickTip(QString text);
    void slotApplyCancel();
    void slotApplySure();
};

#endif // APPLYFRIEND_H

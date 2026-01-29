#include "global.h"
#include "QStyle"

extern std::function<void(QWidget*)> repolish = [](QWidget *w){
    w->style()->unpolish(w);
    w->style()->polish(w);
};

// [Configuration] 定义并初始化
QString gate_url_prefix = "";

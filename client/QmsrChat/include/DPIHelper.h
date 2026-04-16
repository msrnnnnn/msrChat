/**
 * @file DPIHelper.h
 * @brief DPI 适配工具类
 * @details 提供基于屏幕 DPI 的动态尺寸计算和响应式布局辅助功能
 */
#ifndef DPIHELPER_H
#define DPIHELPER_H

#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>
#include <QFontMetrics>
#include <QScreen>
#include <QSize>
#include <QWidget>

class DPIHelper
{
public:
    static DPIHelper &instance()
    {
        static DPIHelper instance;
        return instance;
    }

    qreal scaleFactor() const
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        if (QCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling))
        {
            return QGuiApplication::primaryScreen()->devicePixelRatio();
        }
#endif
        return 1.0;
    }

    int scaled(int baseValue) const
    {
        return qRound(baseValue * scaleFactor());
    }

    int scaledWidth(int baseWidth) const
    {
        return scaled(baseWidth);
    }

    int scaledHeight(int baseHeight) const
    {
        return scaled(baseHeight);
    }

    QSize scaledSize(int baseWidth, int baseHeight) const
    {
        return QSize(scaled(baseWidth), scaled(baseHeight));
    }

    QSize scaledSize(QSize baseSize) const
    {
        return scaledSize(baseSize.width(), baseSize.height());
    }

    int dpiScaleX() const
    {
        return scaled(100);
    }

    int dpiScaleY() const
    {
        return scaled(100);
    }

    void applySizePolicy(QWidget *widget, bool horizontalStretch = true, bool verticalStretch = true)
    {
        if (!widget)
            return;

        QSizePolicy policy;
        if (horizontalStretch)
        {
            policy.setHorizontalPolicy(QSizePolicy::Expanding);
        }
        else
        {
            policy.setHorizontalPolicy(QSizePolicy::Preferred);
        }

        if (verticalStretch)
        {
            policy.setVerticalPolicy(QSizePolicy::Expanding);
        }
        else
        {
            policy.setVerticalPolicy(QSizePolicy::Preferred);
        }

        policy.setHorizontalStretch(0);
        policy.setVerticalStretch(0);
        widget->setSizePolicy(policy);
    }

    void constrainSize(QWidget *widget, int minW, int minH, int maxW = -1, int maxH = -1)
    {
        if (!widget)
            return;

        widget->setMinimumSize(scaled(minW), scaled(minH));

        if (maxW > 0 && maxH > 0)
        {
            widget->setMaximumSize(scaled(maxW), scaled(maxH));
        }
        else if (maxW > 0)
        {
            widget->setMaximumWidth(scaled(maxW));
        }
        else if (maxH > 0)
        {
            widget->setMaximumHeight(scaled(maxH));
        }
    }

    void centerOnScreen(QWidget *widget)
    {
        if (!widget)
            return;

        QScreen *screen = QApplication::primaryScreen();
        if (screen)
        {
            QRect screenGeometry = screen->geometry();
            int x = (screenGeometry.width() - widget->width()) / 2;
            int y = (screenGeometry.height() - widget->height()) / 2;
            widget->move(x, y);
        }
    }

    int getScreenWidth() const
    {
        QScreen *screen = QApplication::primaryScreen();
        return screen ? screen->geometry().width() : 800;
    }

    int getScreenHeight() const
    {
        QScreen *screen = QApplication::primaryScreen();
        return screen ? screen->geometry().height() : 600;
    }

    int fontHeight() const
    {
        return QApplication::fontMetrics().height();
    }

    int fontWidth(const QString &text) const
    {
        return QApplication::fontMetrics().horizontalAdvance(text);
    }

private:
    DPIHelper()
    {
        qDebug() << "[DPIHelper] Initialized with scale factor:" << scaleFactor();
    }

    DPIHelper(const DPIHelper &) = delete;
    DPIHelper &operator=(const DPIHelper &) = delete;
};

#define DPI DPIHelper::instance()

#endif // DPIHELPER_H

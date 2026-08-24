// linkbutton.cpp
#include "linkbutton.h"
#include "whitelabeltheme.h"
#include "theme.h"

namespace OCC {
    LinkButton::LinkButton(QWidget* parent)
        : QLabel(parent)
    {
        customizeStyle();
        setCursor(Qt::PointingHandCursor);
    }

    void LinkButton::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QLabel::mousePressEvent(event);
    }

    void LinkButton::changeEvent(QEvent* event)
    {
        switch (event->type()) {
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::ThemeChange:
            customizeStyle();
            break;
        default:
            break;
        }

        QLabel::changeEvent(event);
    }

    void LinkButton::customizeStyle()
    {
        setStyleSheet(QStringLiteral("QLabel { color: %1; text-decoration: underline; font-size: %2; font-weight: %3; }")
            .arg(WLTheme.settingsLinkColor()
                , WLTheme.settingsTextSize()
                , WLTheme.settingsTitleWeight600()
            ));
    }
}
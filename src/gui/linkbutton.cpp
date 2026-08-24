// linkbutton.cpp
#include "linkbutton.h"
#include "whitelabeltheme.h"
#include "theme.h"
#include <QScopedValueRollback>

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
        // setStyleSheet() below triggers a StyleChange/PaletteChange event, which changeEvent()
        // turns back into a customizeStyle() call - guard against that reentrancy the same way
        // SettingsDialog::customizeStyle() does, or this recurses until the process crashes.
        if (_updatingStyle) {
            return;
        }
        const QScopedValueRollback<bool> updatingStyle(_updatingStyle, true);

        setStyleSheet(QStringLiteral("QLabel { color: %1; text-decoration: underline; font-size: %2; font-weight: %3; }")
            .arg(WLTheme.settingsLinkColor()
                , WLTheme.settingsTextSize()
                , WLTheme.settingsTitleWeight600()
            ));
    }
}
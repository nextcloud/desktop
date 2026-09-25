/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSystemTrayIcon>
#include <QtGlobal>
#ifdef Q_OS_MACOS
#include <QPoint>
#include <QRect>
#endif

namespace OCC
{

/** @brief Defines which tray-icon activations open the primary tray popup. */
class TrayActivationPolicy
{
public:
    /** @brief Whether the activation should open or toggle the primary tray popup. */
    [[nodiscard]] static constexpr bool opensPrimaryPopup(const QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger) {
            return true;
        }

#ifdef Q_OS_MACOS
        return reason == QSystemTrayIcon::Context;
#else
        return false;
#endif
    }

    /** @brief Preserve visibility from before AppKit dispatches a mouse press. */
    void recordMouseDown(const quint64 eventNumber, const bool popupVisible)
    {
        _eventNumber = eventNumber;
        _popupVisibleAtMouseDown = popupVisible;
    }

    [[nodiscard]] bool popupWasVisibleAtMouseDown(const quint64 eventNumber) const
    {
        return _eventNumber == eventNumber && _popupVisibleAtMouseDown;
    }

#ifdef Q_OS_MACOS
    [[nodiscard]] static bool keepPopupOpenOnFocusLoss(const bool wasVisible, const QRect &trayIconRect, const QPoint &cursorPosition)
    {
        return wasVisible && trayIconRect.isValid() && trayIconRect.contains(cursorPosition);
    }
#endif

private:
    quint64 _eventNumber = 0;
    bool _popupVisibleAtMouseDown = false;
};

}

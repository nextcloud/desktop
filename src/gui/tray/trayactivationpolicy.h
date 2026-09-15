/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSystemTrayIcon>

namespace OCC {

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
};

}

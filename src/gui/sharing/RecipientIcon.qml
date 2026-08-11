/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma Singleton

import QtQuick
import QtQuick.Controls

QtObject {
    function source(svgUrl: string, light: string, dark: string): string {
        if (svgUrl) {
            return svgUrl
        }
        const icon = Application.styleHints.colorScheme === Qt.ColorScheme.Dark ? dark : light
        return icon ? `image://tray-image-provider/${icon}` : ""
    }
}

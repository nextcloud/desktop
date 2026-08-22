/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma Singleton

import QtQuick

QtObject {
    readonly property bool darkMode: false
    readonly property color wizardHeaderBackgroundColor: "#0082c9"
    readonly property color wizardHeaderTitleColor: "#ffffff"
    readonly property url stateOnlineImageSource: ""
    readonly property url stateOfflineImageSource: ""
}

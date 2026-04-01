/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

ApplicationWindow {
    id: root

    property var accountState
    property string localPath: ""

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300

    title: qsTr("The new sharing dialog!")
}

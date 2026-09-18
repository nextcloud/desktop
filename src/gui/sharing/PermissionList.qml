/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

import Style

SettingsPanel {
    id: root

    property alias model: permissionList.model

    signal permissionToggled(string permissionClass, bool enabled)

    implicitHeight: permissionList.contentHeight

    ListView {
        id: permissionList

        anchors.fill: parent
        objectName: "permissionList"
        interactive: false
        spacing: 0

        delegate: SwitchDelegate {
            required property var model

            objectName: "permissionSwitch"
            background: null
            width: ListView.view.width
            text: model.label
            checked: model.enabled
            enabled: model.available

            onToggled: root.permissionToggled(model.className, checked)
        }
    }
}

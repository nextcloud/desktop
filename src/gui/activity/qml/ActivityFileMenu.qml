/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../../tray"

AutoSizingMenu {
    id: root

    required property string filePath
    required property bool serverHasIntegration
    required property int itemFontPixelSize

    signal fileDetailsRequested(string filePath)
    signal fileActionsRequested(string filePath)

    closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape
    height: implicitHeight

    Action {
        id: fileDetailsAction
        objectName: "fileDetailsAction"
        text: qsTr("File details")
        onTriggered: root.fileDetailsRequested(root.filePath)
    }

    Action {
        id: fileActionsAction
        objectName: "fileActionsAction"
        text: qsTr("File actions")
        onTriggered: root.fileActionsRequested(root.filePath)
    }

    MenuItem {
        action: fileDetailsAction
        font.pixelSize: root.itemFontPixelSize
        hoverEnabled: true
    }

    MenuItem {
        action: fileActionsAction
        visible: root.serverHasIntegration
        height: visible ? implicitHeight : 0
        font.pixelSize: root.itemFontPixelSize
        hoverEnabled: true
    }
}

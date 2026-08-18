/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../../tray"

Menu {
    id: root

    required property string filePath
    required property bool serverHasIntegration
    required property int itemFontPixelSize

    signal fileDetailsRequested(string filePath)
    signal fileActionsRequested(string filePath)

    closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

    property Action fileDetailsAction: Action {
        id: fileDetailsAction
        objectName: "fileDetailsAction"
        text: qsTr("File details")
        onTriggered: root.fileDetailsRequested(root.filePath)
    }

    property Action fileActionsAction: Action {
        id: fileActionsAction
        objectName: "fileActionsAction"
        text: qsTr("File actions")
        onTriggered: root.fileActionsRequested(root.filePath)
    }

    MenuItem {
        id: fileDetailsMenuItem
        action: root.fileDetailsAction
    }

    MenuItem {
        id: fileActionsMenuItem
        action: root.fileActionsAction
        visible: root.serverHasIntegration
    }
}

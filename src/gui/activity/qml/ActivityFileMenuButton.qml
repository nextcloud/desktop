/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

Button {
    id: root

    required property string filePath
    required property bool serverHasIntegration
    required property int itemFontPixelSize
    required property int buttonWidth
    required property int buttonHeight
    required property int buttonIconSize

    readonly property alias menu: fileMenu

    signal fileDetailsRequested(string filePath)
    signal fileActionsRequested(string filePath)

    width: buttonWidth
    height: buttonHeight

    icon.name: "view-more-symbolic"
    icon.source: "image://svgimage-custom-color/more.svg/" + palette.buttonText
    icon.width: buttonIconSize
    icon.height: buttonIconSize

    ToolTip {
        popupType: Qt.platform.os === "windows" ? Popup.Item : Popup.Native
        text: qsTr("Open file details")
        visible: parent.hovered
    }

    display: Button.IconOnly
    onClicked: fileMenu.visible ? fileMenu.close() : fileMenu.popup()

    ActivityFileMenu {
        id: fileMenu

        filePath: root.filePath
        serverHasIntegration: root.serverHasIntegration
        itemFontPixelSize: root.itemFontPixelSize

        onFileDetailsRequested: path => root.fileDetailsRequested(path)
        onFileActionsRequested: path => root.fileActionsRequested(path)
    }
}

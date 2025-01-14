/*
 * Copyright (C) 2020 by Nicolas Fella <nicolas.fella@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQml
import QtQml.Models
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

// Custom qml modules are in /theme (and included by resources.qrc)
import Style
import com.ionos.hidrivenext.desktopclient

Button {
    id: root

    display: AbstractButton.TextUnderIcon
    flat: true
    hoverEnabled: Style.hoverEffectsEnabled

    icon.width: Style.headerButtonIconSize
    icon.height: Style.headerButtonIconSize

    Layout.alignment: Qt.AlignRight
    Layout.preferredWidth: Style.sesHeaderButtonWidth
    Layout.preferredHeight: Style.sesHeaderButtonHeight

    property bool isHovered: root.hovered || root.visualFocus
    property bool isActive: root.pressed

    background: Rectangle {
        width: Style.sesHeaderButtonWidth
        height: Style.sesHeaderButtonHeight
        color: root.isActive ? Style.sesButtonPressed : root.isHovered ? Style.sesAccountMenuHover : "transparent"
        radius: Style.sesCornerRadius
    }

    contentItem: Item {
        id: rootContent

        Image {
            id: buttonIcon
            anchors.horizontalCenter: rootContent.horizontalCenter
            anchors.top: rootContent.top
            anchors.topMargin: 10

            property int imageWidth: root.icon.width
            property int imageHeight: root.icon.height
            cache: true

            source: root.icon.source
            sourceSize {
                width: imageWidth
                height: imageHeight
            }

            width: imageWidth
            height: imageHeight

            anchors.verticalCenter: parent
        }

        Text {
            anchors.horizontalCenter: buttonIcon.horizontalCenter
            anchors.top: buttonIcon.bottom
            anchors.topMargin: 5
            font: root.font
            text: root.text
            color: Style.sesTrayFontColor
        }
    }
}

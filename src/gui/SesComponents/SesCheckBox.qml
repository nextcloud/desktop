/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

import Style 1.0

CheckBox {
    id: root

    property color accentColor: Style.ncBlue

    hoverEnabled: false
    palette.base: Style.sesBackgroundColor

    contentItem: Text {
        text: root.text
        color: Style.sesTrayFontColor
        font: root.font
        leftPadding: root.indicator.width + root.spacing
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle {
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: 20
        implicitHeight: 20
        radius: 3
        border.width: Style.thickBorderWidth
        border.color: root.checked ? root.accentColor : Style.sesTrayInputField
        color: root.checked ? root.accentColor : Style.sesBackgroundColor

        Image {
            anchors.centerIn: parent
            width: parent.width * 0.65
            height: width
            visible: root.checked
            fillMode: Image.PreserveAspectFit
            source: "image://svgimage-custom-color/check.svg/white"
            sourceSize: Qt.size(width, height)
        }
    }
}

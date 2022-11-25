/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import QtQuick 2.6
import QtQuick.Dialogs 1.3
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import Style 1.0

import "./tray"

AbstractButton {
    id: root

    property string secondaryText: ""
    readonly property bool showBorder: hovered || checked

    readonly property bool hasImage: root.icon.source !== ""
    readonly property bool hasSecondaryText: root.secondaryText !== ""
    readonly property bool hasPrimaryText: root.text !== ""

    hoverEnabled: true
    padding: Style.standardSpacing

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        color: Style.buttonBackgroundColor
        border.color: root.checked ? Style.ncBlue : Style.menuBorder
        border.width: root.showBorder ? Style.thickBorderWidth : 0
    }

    contentItem: GridLayout {
        columns: root.hasImage ? 2 : 1
        rows: 2
        columnSpacing: Style.standardSpacing
        rowSpacing: Style.standardSpacing / 2

        Image {
            Layout.column: 0
            Layout.columnSpan: !root.hasPrimaryText && !root.hasSecondaryText ? 2 : 1
            Layout.row: 0
            Layout.rowSpan: 2
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            source: root.icon.source
            visible: root.hasImage
        }

        EnforcedPlainTextLabel {
            Layout.column: root.hasImage ? 1 : 0
            Layout.columnSpan: root.hasImage ? 1 : 2
            Layout.row: 0
            Layout.rowSpan: root.hasSecondaryText ? 1 : 2
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            text: root.text
            wrapMode: Text.Wrap
            color: root.colored ? Style.ncHeaderTextColor : Style.ncTextColor
        }

        EnforcedPlainTextLabel {
            Layout.column: root.hasImage ? 1 : 0
            Layout.columnSpan: root.hasImage ? 1 : 2
            Layout.row: 1
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            text: root.secondaryText
            wrapMode: Text.Wrap
            color: Style.ncSecondaryTextColor
            visible: root.hasSecondaryText
        }
    }
}

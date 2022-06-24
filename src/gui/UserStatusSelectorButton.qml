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

AbstractButton {
    id: root

    property string secondaryText: ""
    property bool colored: false
    property bool primary: false
    property bool highlighted: false
    readonly property bool showBorder: hovered || highlighted || checked

    hoverEnabled: true
    padding: Style.standardSpacing

    background: Rectangle {
        radius: root.primary ? Style.veryRoundedButtonRadius : Style.mediumRoundedButtonRadius
        color: root.colored ? Style.ncBlue : Style.buttonBackgroundColor
        opacity: root.colored && root.hovered ? Style.hoverOpacity : 1.0
        border.color: Style.ncBlue
        border.width: root.showBorder ? root.primary ? Style.normalBorderWidth : Style.thickBorderWidth : 0
    }

    contentItem: GridLayout {
        columns: 2
        rows: 2
        columnSpacing: Style.standardSpacing
        rowSpacing: Style.standardSpacing / 2

        Image {
            Layout.column: 0
            Layout.columnSpan: root.text === "" && root.secondaryText == "" ? 2 : 1
            Layout.row: 0
            Layout.rowSpan: 2
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter


            source: root.icon.source
            visible: root.icon.source !== ""
        }

        Label {
            Layout.column: root.icon.source === "" ? 0 : 1
            Layout.columnSpan: root.icon.source === "" ? 2 : 1
            Layout.row: 0
            Layout.rowSpan: root.secondaryText === "" ? 2 : 1
            Layout.fillWidth: true
            horizontalAlignment: root.primary ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            text: root.text
            wrapMode: Text.Wrap
            color: root.colored ? Style.ncHeaderTextColor : Style.ncTextColor
            font.bold: root.primary
        }

        Label {
            Layout.column: root.icon.source === "" ? 0 : 1
            Layout.columnSpan: root.icon.source === "" ? 2 : 1
            Layout.row: 1
            Layout.fillWidth: true
            horizontalAlignment: root.primary ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            text: root.secondaryText
            wrapMode: Text.Wrap
            color: Style.ncSecondaryTextColor
            visible: root.secondaryText !== ""
        }
    }
}

/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Style

import "./tray"

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
        color: root.colored ? Style.ncBlue : palette.button
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

        EnforcedPlainTextLabel {
            Layout.column: root.icon.source === "" ? 0 : 1
            Layout.columnSpan: root.icon.source === "" ? 2 : 1
            Layout.row: 0
            Layout.rowSpan: root.secondaryText === "" ? 2 : 1
            Layout.fillWidth: true
            horizontalAlignment: root.primary ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            text: root.text
            wrapMode: Text.Wrap
            font.bold: root.primary
        }

        EnforcedPlainTextLabel {
            Layout.column: root.icon.source === "" ? 0 : 1
            Layout.columnSpan: root.icon.source === "" ? 2 : 1
            Layout.row: 1
            Layout.fillWidth: true
            horizontalAlignment: root.primary ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            text: root.secondaryText
            wrapMode: Text.Wrap
            visible: root.secondaryText !== ""
        }
    }
}

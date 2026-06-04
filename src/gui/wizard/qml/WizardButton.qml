/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as BasicControls
import Style

BasicControls.Button {
    id: root

    property bool primary: false
    property string iconSource: ""
    property bool iconBeforeText: false
    property string textSuffix: ""
    readonly property color primaryColor: Style.wizardPrimaryButtonBackground
    readonly property color primaryPressedColor: Style.wizardPrimaryButtonPressed
    readonly property color secondaryColor: Style.wizardSecondaryButtonBackground
    readonly property color secondaryPressedColor: Style.wizardSecondaryButtonPressed
    readonly property color secondaryBorderColor: Style.wizardSecondaryButtonBorder
    readonly property color disabledColor: Style.wizardDisabledButtonBackground
    readonly property color disabledBorderColor: Style.wizardDisabledButtonBorder

    implicitHeight: 36
    leftPadding: 18
    rightPadding: 18
    font.pixelSize: Style.pixelSize + 3
    font.weight: Font.Medium
    Accessible.role: Accessible.Button
    Accessible.name: textSuffix === "" ? text : text + " " + textSuffix

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow

            anchors.centerIn: parent
            spacing: 6

            Image {
                visible: root.iconSource !== "" && root.iconBeforeText
                source: root.iconSource
                sourceSize.width: 16
                sourceSize.height: 16
                width: visible ? 16 : 0
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: root.textSuffix === "" ? root.text : root.text + " " + root.textSuffix
                font: root.font
                color: root.enabled
                    ? (root.primary ? Style.wizardSelectedText : root.palette.buttonText)
                    : Style.wizardDisabledText
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
            }

            Image {
                visible: root.iconSource !== "" && !root.iconBeforeText
                source: root.iconSource
                sourceSize.width: 16
                sourceSize.height: 16
                width: visible ? 16 : 0
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    background: Rectangle {
        radius: 8
        border.width: root.primary ? 0 : 1
        border.color: root.enabled ? root.secondaryBorderColor : root.disabledBorderColor
        color: {
            if (!root.enabled) {
                return root.disabledColor
            }
            if (root.primary) {
                return root.down ? root.primaryPressedColor : root.primaryColor
            }
            return root.down
                ? root.secondaryPressedColor
                : root.secondaryColor
        }
    }
}

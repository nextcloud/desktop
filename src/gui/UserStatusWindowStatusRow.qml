/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls
import QtQuick.Layouts

import Style
import "./tray"

BasicControls.Button {
    id: root

    property string secondaryText: ""
    property url iconSource: ""
    property bool selected: false

    hoverEnabled: true
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    implicitHeight: 44

    Accessible.role: Accessible.Button
    Accessible.name: secondaryText === "" ? text : qsTr("%1, %2").arg(text).arg(secondaryText)

    background: Rectangle {
        radius: 8
        color: root.hovered ? Style.wizardSelectedBackground : Style.wizardRowBackground
        border.width: root.selected || root.activeFocus ? 2 : 0
        border.color: root.selected ? Style.ncBlue
            : root.activeFocus ? Style.ncBlue
            : "transparent"
    }

    contentItem: Item {
        implicitWidth: Math.max(0, root.width - root.leftPadding - root.rightPadding)
        implicitHeight: contentLayout.implicitHeight

        RowLayout {
            id: contentLayout

            anchors.fill: parent
            spacing: 12

            Image {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                source: root.iconSource
                sourceSize: Qt.size(18, 18)
                fillMode: Image.PreserveAspectFit
                visible: root.iconSource.toString() !== ""
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: root.secondaryText === ""
                Layout.preferredWidth: root.secondaryText === "" ? implicitWidth : 160
                text: root.text
                color: root.enabled ? Style.wizardPrimaryText : Style.wizardDisabledText
                font.pixelSize: Style.pixelSize + 3
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                visible: root.secondaryText !== ""
                text: root.secondaryText
                color: root.enabled ? Style.wizardSecondaryText : Style.wizardDisabledText
                font.pixelSize: Style.pixelSize + 1
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }
    }
}

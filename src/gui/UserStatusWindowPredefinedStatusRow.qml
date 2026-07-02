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

    property string emoji: ""
    property string statusText: ""
    property string clearAtText: ""
    property bool selected: false

    hoverEnabled: true
    leftPadding: 0
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    implicitHeight: 30

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("%1, clears %2").arg(statusText).arg(clearAtText)

    background: Rectangle {
        radius: 8
        color: root.hovered ? Style.wizardRowBackground : "transparent"
        border.width: root.selected || root.activeFocus ? 2 : 0
        border.color: root.activeFocus ? Style.ncBlue
            : root.selected ? Style.ncBlue
            : "transparent"
    }

    contentItem: Item {
        implicitWidth: Math.max(0, root.width - root.leftPadding - root.rightPadding)
        implicitHeight: contentLayout.implicitHeight

        RowLayout {
            id: contentLayout

            anchors.fill: parent
            spacing: 8

            EnforcedPlainTextLabel {
                Layout.preferredWidth: 36
                text: root.emoji
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Style.pixelSize + 3
            }

            EnforcedPlainTextLabel {
                Layout.maximumWidth: implicitWidth
                text: root.statusText
                color: Style.wizardPrimaryText
                font.pixelSize: Style.pixelSize + 1
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: qsTr("- %1").arg(root.clearAtText)
                color: Style.wizardSecondaryText
                font.pixelSize: Style.pixelSize
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }
    }
}

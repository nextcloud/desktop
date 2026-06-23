/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import "../../tray"

Control {
    id: root

    property alias title: titleLabel.text
    property alias description: descriptionLabel.text
    property string iconSource: ""
    property bool selected: false
    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color hintTextColor: Style.wizardSecondaryText

    signal clicked()

    implicitHeight: descriptionLabel.text === "" ? 42 : 56
    padding: 10

    background: Rectangle {
        radius: 6
        border.width: 1
        border.color: !root.enabled ? Style.wizardRowDisabledBorder : root.selected ? Style.wizardSelectedBorder : Style.wizardRowBorder
        color: !root.enabled ? Style.wizardRowDisabledBackground : root.selected
            ? Style.wizardSelectedBackground
            : Style.wizardRowBackground
    }

    contentItem: RowLayout {
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            Layout.alignment: Qt.AlignVCenter
            radius: width / 2
            border.width: 2
            border.color: root.enabled ? Style.wizardRadioAccent : Style.wizardRadioDisabled
            color: "transparent"

            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: width / 2
                color: Style.wizardRadioAccent
                visible: root.selected && root.enabled
            }
        }

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            EnforcedPlainTextLabel {
                id: titleLabel
                Layout.fillWidth: true
                color: root.enabled ? root.primaryTextColor : root.hintTextColor
                font.bold: true
                font.pixelSize: Style.pixelSize + 1
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                id: descriptionLabel
                visible: text !== ""
                Layout.fillWidth: true
                color: root.hintTextColor
                font.pixelSize: Style.pixelSize
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}

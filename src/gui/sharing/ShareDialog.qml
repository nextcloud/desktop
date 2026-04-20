/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui/tray"

ApplicationWindow {
    id: root
    visible: true

    property var accountState
    property QtObject sharingManager
    property string localPath: ""
    property string shortLocalPath: ""

    readonly property int windowRadius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius

    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300

    title: qsTr("Share \"%1\"").arg(root.shortLocalPath)

    ColumnLayout {
        id: windowContent
        anchors.fill: parent
        anchors.margins: Style.standardSpacing

        RowLayout {
            id: windowHeader
            Layout.fillWidth: true
            spacing: Style.standardSpacing

            EnforcedPlainTextLabel {
                id: headerLocalPath
                text: qsTr("Share \"%1\"").arg(root.shortLocalPath)
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: Style.pixelSize
                color: palette.text
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
            }

            Button {
                id: settingsButton
                flat: true
                padding: Style.extraSmallSpacing
                spacing: 0
                icon.source: "image://svgimage-custom-color/settings.svg/" + palette.windowText
                icon.width: Style.extraSmallIconSize
                icon.height: Style.extraSmallIconSize
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                Layout.rightMargin: Style.extraSmallSpacing
                Layout.topMargin: Style.extraSmallSpacing
                background: Rectangle {
                    color: "transparent"
                    radius: root.windowRadius
                    border.width: closeButton.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.fill: parent
                    Layout.margins: Style.extraSmallSpacing
                }
            }

            Button {
                id: closeButton
                flat: true
                padding: Style.extraSmallSpacing
                spacing: 0
                icon.source: "image://svgimage-custom-color/close.svg/" + palette.windowText
                icon.width: Style.extraSmallIconSize
                icon.height: Style.extraSmallIconSize
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                Layout.rightMargin: Style.extraSmallSpacing
                Layout.topMargin: Style.extraSmallSpacing
                onClicked: root.close()
                background: Rectangle {
                    color: "transparent"
                    radius: root.windowRadius
                    border.width: closeButton.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.fill: parent
                    Layout.margins: Style.extraSmallSpacing
                }
            }
        }

        TabBar {
            id: bar
            Layout.fillWidth: true
            TabButton {
                id: viewInvitedPeople
                text: qsTr("Invited people")
            }
            TabButton {
                id: viewAnyone
                text: qsTr("Anyone")
            }
        }

        StackLayout {
            currentIndex: bar.currentIndex
            ColumnLayout {
                Label {
                    text: qsTr("Add people")
                }
                Label {
                    text: qsTr("Participants")
                }
                TextArea {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Note to recipients")
                }
            }
            ColumnLayout {
                Label {
                    text: qsTr("Anyone with the link")
                }
                TextArea {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Note to recipients")
                }
            }
        }

        RowLayout {
            Button {
                Layout.fillWidth: true

                text: qsTr("Copy link")
            }
            Button {
                Layout.fillWidth: true

                text: qsTr("Send")
                visible: !viewAnyone.checked
                enabled: !viewAnyone.checked
            }
        }
    }


}

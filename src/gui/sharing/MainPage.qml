/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

Page {
    id: root

    property var accountState
    property QtObject sharingManager
    property string localPath: ""
    property string shortLocalPath: ""

    title: qsTr("Share \"%1\"").arg(root.shortLocalPath)

    SharingModel {
        id: theModel
    }

    ColumnLayout {
        id: windowContent
        anchors.fill: parent
        anchors.margins: Style.standardSpacing

        // TODO: the contents should be presented through a viewmodel

        RowLayout {
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
        }

        StackLayout {
            currentIndex: bar.currentIndex
            ColumnLayout {
                EnforcedPlainTextLabel {
                    text: qsTr("Add people")
                }
                EnforcedPlainTextLabel {
                    text: qsTr("Participants")
                }
                TextArea {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Note to recipients")
                }
                Repeater {
                    model: theModel

                    delegate: EnforcedPlainTextLabel {
                        required property string label
                        text: label
                    }
                }
            }
            ColumnLayout {
                EnforcedPlainTextLabel {
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

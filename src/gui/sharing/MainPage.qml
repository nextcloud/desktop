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
    required property SharingModel sharingModel

    title: qsTr("Share \"%1\"").arg(root.shortLocalPath)

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
            ScrollView {
                ScrollBar.vertical.policy: propertyList.contentHeight > propertyList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                contentWidth: availableWidth
                rightPadding: ScrollBar.vertical.policy == ScrollBar.AlwaysOn ? ScrollBar.vertical.width + Style.standardSpacing : 0

                ListView {
                    id: propertyList
                    clip: true

                    model: root.sharingModel

                    delegate: FieldDelegate {
                        width: propertyList.contentItem.width
                    }
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

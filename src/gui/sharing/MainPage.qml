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
    required property list<string> recipientTypes

    title: qsTr("Share \"%1\"").arg(root.shortLocalPath)

    ColumnLayout {
        id: windowContent
        anchors.fill: parent

        ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            ScrollBar.vertical.policy: propertyList.contentHeight > propertyList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            contentWidth: availableWidth
            contentHeight: availableHeight
            rightPadding: ScrollBar.vertical.policy == ScrollBar.AlwaysOn ? ScrollBar.vertical.width + Style.standardSpacing : 0

            ListView {
                id: propertyList
                clip: true

                model: SharingFilterModel {
                    filterType: SharingFilterModel.General
                    sourceModel: root.sharingModel
                    recipientTypes: root.recipientTypes
                }

                delegate: FieldDelegate {
                    width: propertyList.contentItem.width
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
                visible: root.recipientTypes.length > 1
                enabled: root.recipientTypes.length > 1
            }
        }
    }
}

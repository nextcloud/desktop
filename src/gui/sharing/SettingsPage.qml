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

    property string localPath: ""
    property string shortLocalPath: ""
    required property SharingController sharingController
    required property list<string> recipientTypes

    title: qsTr("Sharing settings")

    ColumnLayout {
        id: windowContent
        anchors.fill: parent

        ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            ScrollBar.vertical.policy: propertyList.contentHeight > propertyList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            contentWidth: availableWidth
            rightPadding: ScrollBar.vertical.policy == ScrollBar.AlwaysOn ? ScrollBar.vertical.width + Style.standardSpacing : 0

            ListView {
                id: propertyList
                clip: true

                // model: SharingFilterModel {
                //     filterType: SharingFilterModel.Settings
                //     sourceModel: root.sharingModel
                //     recipientTypes: root.recipientTypes
                // }
                model: PropertyModel {
                    share: root.sharingController.share
                }


                delegate: FieldDelegate {
                    account: root.sharingController.account
                    width: propertyList.contentItem.width
                }
            }
        }
    }
}

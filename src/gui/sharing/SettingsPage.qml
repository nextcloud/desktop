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

    title: qsTr("Sharing settings")

    ScrollView {
        anchors.fill: parent

        ScrollBar.vertical.policy: propertyList.contentHeight > propertyList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        contentWidth: availableWidth
        rightPadding: ScrollBar.vertical.policy == ScrollBar.AlwaysOn ? ScrollBar.vertical.width + Style.standardSpacing : 0

        ListView {
            id: propertyList
            clip: true

            model: sharingModel

            delegate: FieldDelegate {
                width: propertyList.contentItem.width
            }
        }
    }
}

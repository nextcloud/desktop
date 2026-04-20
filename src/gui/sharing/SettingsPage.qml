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

    title: qsTr("Sharing settings")

    ColumnLayout {
        id: windowContent
        anchors.fill: parent
        anchors.margins: Style.standardSpacing

        ColumnLayout {
            // TODO: these should be presented through a viewmodel
            Label {
                text: qsTr("Show files in grid view")
            }
        }
    }
}

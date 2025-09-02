/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

ApplicationWindow {
    id: root
    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300
    LayoutMirroring.childrenInherit: true
    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft

    property var accountState: ({})
    property string localPath: ""

    title: qsTr("File actions for %1").arg(root.localPath)

    Component {
        id: fileActionsDelegate

        Item {
            id: fileActionsItem
            width: parent.width
            height: 40

            required property string type
            required property string name
            required property string url

            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 5
                height: implicitHeight

                Text {
                    text: fileActionsItem.type
                    color: Style.accentColor
                    font.pixelSize: Style.pixelSize
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: fileActionsItem.name
                    color: Style.accentColor
                    font.pixelSize: Style.pixelSize
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: fileActionsItem.url
                    color: Style.accentColor
                    font.pixelSize: Style.pixelSize
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    EndpointModel {
        id: endpointModel
        accountState: root.accountState
        localPath: root.localPath
    }

    ListView {
        id: fileActionsView
        model: endpointModel
        delegate: fileActionsDelegate

        anchors.fill: parent
        anchors.margins: 10
    }

}

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
import "../tray"

ApplicationWindow {
    id: root
    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300
    LayoutMirroring.childrenInherit: true
    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    flags: Qt.Window
    color: Style.currentUserHeaderColor

    property var accountState: ({})
    property string localPath: ""

    title: qsTr("File actions for %1").arg(root.localPath)

    EndpointModel {
        id: endpointModel
        accountState: root.accountState
        localPath: root.localPath
    }

    RowLayout {
        spacing: 8
        Layout.fillWidth: true

        Image {
            source: "image://svgimage-custom-color/folder.svg/" + palette.windowText
            Layout.minimumWidth: Style.headerButtonIconSize
            Layout.minimumHeight: Style.headerButtonIconSize
        }

        EnforcedPlainTextLabel {
            text: root.localPath
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
        }

        Button {
            icon.source: "image://svgimage-custom-color/add.svg/" + palette.windowText
            icon.width: Style.activityListButtonIconSize
            icon.height: Style.activityListButtonIconSize
            Layout.minimumWidth: Style.activityListButtonWidth
            Layout.minimumHeight: Style.activityListButtonHeight
        }

        Button {
            icon.source: "image://svgimage-custom-color/close.svg/" + palette.windowText
            icon.width: Style.activityListButtonIconSize
            icon.height: Style.activityListButtonIconSize
            Layout.minimumWidth: Style.activityListButtonWidth
            Layout.minimumHeight: Style.activityListButtonHeight
        }
    }

    Component {
        id: fileActionsDelegate

        Item {
            id: fileActionsItem
            width: parent.width
            height: 40

            required property string name

            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 5
                height: implicitHeight

                Button {
                    icon.source: "image://svgimage-custom-color/files.svg/" + palette.windowText
                    text: fileActionsItem.name
                    font.pixelSize: Style.pixelSize
                    height: implicitHeight
                    onClicked: endpointModel.createRequest(endpointModel.index)
                }
            }
        }
    }

    ListView {
        id: fileActionsView
        model: endpointModel
        delegate: fileActionsDelegate

        anchors.fill: parent
        anchors.margins: 10
    }

}

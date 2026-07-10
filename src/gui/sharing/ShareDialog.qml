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
import "qrc:/qml/src/gui/tray"

ApplicationWindow {
    id: root
    visible: true

    property var account
    property string localPath: ""
    property string shortLocalPath: root.localPath.split("/").reverse()[0]
    property string fileId: ""

    readonly property int windowRadius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius

    ButtonGroup {
        id: shareType
    }

    SharingController {
        id: sharingController
        account: root.account
    }

    Component.onCompleted: {
        sharingController.createShare(root.fileId)
    }

    width: 400
    height: 500
    minimumWidth: 300
    minimumHeight: 300

    title: mainPage.title

    ColumnLayout {
        spacing: Style.standardSpacing
        anchors.fill: parent
        anchors.margins: Style.standardSpacing

        RowLayout {
            // TODO: extract this to a shared component
            id: windowHeader
            Layout.fillWidth: true

            Button {
                id: backButton
                flat: true
                padding: Style.extraSmallSpacing
                spacing: 0
                icon.source: "image://svgimage-custom-color/confirm.svg/" + palette.windowText // TODO: back button icon!
                icon.width: Style.extraSmallIconSize
                icon.height: Style.extraSmallIconSize
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                Layout.rightMargin: Style.extraSmallSpacing
                Layout.topMargin: Style.extraSmallSpacing
                background: Rectangle {
                    color: "transparent"
                    radius: root.windowRadius
                    border.width: parent.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.fill: parent
                    Layout.margins: Style.extraSmallSpacing
                }

                onClicked: stack.pop()
                visible: stack.depth > 1
            }

            ColumnLayout {
                EnforcedPlainTextLabel {
                    id: headerSettings
                    text: stack.currentItem.title
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: Style.pixelSize
                    color: palette.text
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                }
                EnforcedPlainTextLabel {
                    id: headerSettingsLocalPath
                    text: root.shortLocalPath
                    elide: Text.ElideRight
                    font.bold: false
                    font.pixelSize: Style.pixelSize
                    color: palette.text
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                    visible: stack.depth > 1
                }
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
                    border.width: parent.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.fill: parent
                    Layout.margins: Style.extraSmallSpacing
                }

                visible: stack.depth < 2

                onClicked: stack.push(Qt.createComponent("com.nextcloud.desktopclient.sharing", "SettingsPage").createObject(root, {
                    sharingController: sharingController,
                    shortLocalPath: root.shortLocalPath,
                    recipientTypes: Qt.binding(() => root.recipientTypes),
                }))
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
                background: Rectangle {
                    color: "transparent"
                    radius: root.windowRadius
                    border.width: parent.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.fill: parent
                    Layout.margins: Style.extraSmallSpacing
                }

                onClicked: root.close()
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                Layout.fillWidth: true

                id: viewInvitedPeople
                text: qsTr("Invited people")

                checkable: true
                checked: true
                ButtonGroup.group: shareType
            }

            Button {
                Layout.fillWidth: true

                id: viewAnyone
                text: qsTr("Anyone")

                checkable: true
                ButtonGroup.group: shareType

                onCheckedChanged: {
                    const parameters = ["OC\\Core\\Sharing\\Recipient\\TokenShareRecipientType", "somevaluethatislongerthan32characters12345678"]
                    if (checked) {
                        sharingController.addRecipient(...parameters)
                        return
                    }
                    sharingController.removeRecipient(...parameters)
                }
            }
        }

        StackView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: stack

            initialItem: mainPage
        }
    }

    MainPage {
        id: mainPage

        sharingController: sharingController
        localPath: root.localPath
        shortLocalPath: root.shortLocalPath
        isLinkShare: viewAnyone.checked
    }
}

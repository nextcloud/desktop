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
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

WizardStyledWindow {
    id: root
    visible: true

    property var account
    property string localPath: ""
    property string shortLocalPath: root.localPath.split("/").reverse()[0]
    property string fileId: ""

    title: mainPage.title
    width: 400
    height: 500
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight

    SharingController {
        id: sharingController
        account: root.account
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Component.onCompleted: {
        sharingController.initialize(root.fileId)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.wizardWindowMargin
        anchors.rightMargin: Style.wizardWindowMargin
        anchors.topMargin: Style.wizardWindowTopMargin
        anchors.bottomMargin: Style.wizardWindowMargin
        spacing: Style.wizardSectionSpacing

        RowLayout {
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
                    font.pixelSize: Style.wizardHeaderTitleFontPixelSize
                    font.weight: Font.DemiBold
                    color: palette.text
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                }
                EnforcedPlainTextLabel {
                    id: headerSettingsLocalPath
                    text: root.shortLocalPath
                    elide: Text.ElideRight
                    color: Style.wizardSecondaryText
                    font.pixelSize: Style.wizardHeaderAccountServerFontPixelSize
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                    visible: stack.depth > 1
                }
            }

            WizardButton {
                id: settingsButton
                iconSource: "image://svgimage-custom-color/settings.svg/" + palette.windowText

                visible: stack.depth === 2 && stack.currentItem && stack.currentItem.share

                onClicked: stack.push(Qt.createComponent("com.nextcloud.desktopclient.sharing", "SettingsPage").createObject(root, {
                    sharingController: sharingController,
                    share: stack.currentItem.share,
                }))
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
        fileId: root.fileId

        onShareSelected: share => stack.push(Qt.createComponent("com.nextcloud.desktopclient.sharing", "ShareDetailsPage").createObject(root, {
            sharingController: sharingController,
            share: share,
        }))
    }
}

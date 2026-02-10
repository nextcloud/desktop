/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../"
import "../filedetails/"

import Style
import com.nextcloud.desktopclient

Rectangle {
    id: root

    signal featuredAppButtonClicked

    readonly property alias currentAccountHeaderButton: currentAccountHeaderButton
    readonly property alias openLocalFolderButton: openLocalFolderButton
    readonly property alias appsMenu: appsMenu

    color: Style.currentUserHeaderColor

    palette {
        text: Style.currentUserHeaderTextColor
        windowText: Style.currentUserHeaderTextColor
        buttonText: Style.currentUserHeaderTextColor
        button: Style.adjustedCurrentUserHeaderColor
    }

    RowLayout {
        id: trayWindowHeaderLayout

        spacing: 0
        anchors.fill: parent

        CurrentAccountHeaderButton {
            id: currentAccountHeaderButton
            parentBackgroundColor: root.color
            Layout.preferredWidth:  Style.currentAccountButtonWidth
            Layout.fillHeight: true
        }

        // Add space between items
        Item {
            Layout.fillWidth: true
        }

        TrayFoldersMenuButton {
            id: openLocalFolderButton

            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth:  Style.trayWindowHeaderHeight
            Layout.fillHeight: true

            visible: currentUser.hasLocalFolder
            currentUser: UserModel.currentUser
            parentBackgroundColor: root.color

            onClicked: openLocalFolderButton.userHasGroupFolders ? openLocalFolderButton.toggleMenuOpen() : UserModel.openCurrentAccountLocalFolder()

            onFolderEntryTriggered: isGroupFolder ? UserModel.openCurrentAccountFolderFromTrayInfo(fullFolderPath) : UserModel.openCurrentAccountLocalFolder()

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Open local or group folders")
            Accessible.onPressAction: openLocalFolderButton.userHasGroupFolders ? openLocalFolderButton.toggleMenuOpen() : UserModel.openCurrentAccountLocalFolder() 
        }

        HeaderButton {
            id: trayWindowFeaturedAppButton

            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth: Style.trayWindowHeaderHeight
            Layout.fillHeight: true

            visible: UserModel.currentUser.isAssistantEnabled
            icon.source: UserModel.currentUser.featuredAppIcon + "/" + palette.windowText
            onClicked: root.featuredAppButtonClicked()

            Accessible.role: Accessible.Button
            Accessible.name: UserModel.currentUser.featuredAppAccessibleName
            Accessible.onPressAction: trayWindowFeaturedAppButton.clicked() 
        }

        HeaderButton {
            id: trayWindowAppsButton
            icon.source: "image://svgimage-custom-color/more-apps.svg/" + palette.windowText

            onClicked: {
                if(appsMenu.count <= 0) {
                    UserModel.openCurrentAccountServer()
                } else if (appsMenu.visible) {
                    appsMenu.close()
                } else {
                    appsMenu.open()
                }
            }

            Accessible.role: Accessible.ButtonMenu
            Accessible.name: qsTr("More apps")
            Accessible.onPressAction: trayWindowAppsButton.clicked()

            Menu {
                id: appsMenu
                x: Style.trayWindowMenuOffsetX
                y: (trayWindowAppsButton.y + trayWindowAppsButton.height + Style.trayWindowMenuOffsetY)
                width: Style.trayWindowWidth * Style.trayWindowMenuWidthFactor
                height: implicitHeight + y > Style.trayWindowHeight ? Style.trayWindowHeight - y : implicitHeight
                closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                Repeater { 
                    model: UserAppsModel
                    delegate: MenuItem {
                        id: appEntry
                        // HACK: Without creating our own component (and killing native styling)
                        // HACK: we do not have a way to adjust the text and icon spacing.
                        text: "  " + model.appName
                        font.pixelSize: Style.topLinePixelSize
                        icon.source: model.appIconUrl
                        icon.color: palette.windowText
                        onTriggered: UserAppsModel.openAppUrl(appUrl)
                        Accessible.role: Accessible.MenuItem
                        Accessible.name: qsTr("Open %1 in browser").arg(model.appName)
                        Accessible.onPressAction: appEntry.triggered()
                    }
                }
            }
        }
    }
}

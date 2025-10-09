/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../"
import "../filedetails/"

import Style
import com.nextcloud.desktopclient

Button {
    id: root

    readonly property alias userLineInstantiator: userLineInstantiator
    readonly property alias accountMenu: accountMenu
    property color parentBackgroundColor: "transparent"

    display: AbstractButton.IconOnly
    flat: true

    Accessible.role: Accessible.ButtonMenu
    Accessible.name: qsTr("Current account")
    Accessible.onPressAction: root.clicked()

    palette {
        text: Style.currentUserHeaderTextColor
        windowText: Style.currentUserHeaderTextColor
        buttonText: Style.currentUserHeaderTextColor
        button: Style.adjustedCurrentUserHeaderColor
    }

    // We call open() instead of popup() because we want to position it
    // exactly below the dropdown button, not the mouse
    onClicked: {
        syncPauseButton.text = Systray.syncIsPaused ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
        if (accountMenu.visible) {
            accountMenu.close()
        } else {
            accountMenu.open()
        }
    }

    Menu {
        id: accountMenu

        // x coordinate grows towards the right
        // y coordinate grows towards the bottom
        x: (root.x + 2)
        y: (root.y + Style.trayWindowHeaderHeight + 2)

        width: (Style.rootWidth - 2)
        height: Math.min(implicitHeight, maxMenuHeight)
        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

        onClosed: {
            // HACK: reload account Instantiator immediately by restting it - could be done better I guess
            // see also onVisibleChanged above
            userLineInstantiator.active = false;
            userLineInstantiator.active = true;
        }

        Instantiator {
            id: userLineInstantiator
            model: UserModel
            delegate: MenuItem {
                implicitHeight: instantiatedUserLine.height + Style.standardSpacing
                UserLine {
                    id: instantiatedUserLine
                    width: parent.width
                    onShowUserStatusSelector: {
                        userStatusDrawer.openUserStatusDrawer(model.index);
                        accountMenu.close();
                    }
                    onShowUserStatusMessageSelector: {
                        userStatusDrawer.openUserStatusMessageDrawer(model.index);
                        accountMenu.close();
                    }
                    onClicked: UserModel.currentUserId = model.index;
                }
            }
            onObjectAdded: function(index, object) {
                accountMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                accountMenu.removeItem(object)
            }
        }

        MenuItem {
            id: addAccountButton
            hoverEnabled: true
            visible: Systray.enableAddAccount

            readonly property real addAccountIconSize: Style.accountAvatarSize * Style.smallIconScaleFactor
            readonly property real addAccountHorizontalOffset: (Style.accountAvatarSize - addAccountIconSize) / 2

            icon.source: "image://svgimage-custom-color/add.svg/" + palette.windowText
            icon.width: addAccountIconSize
            icon.height: addAccountIconSize
            leftPadding: Style.accountIconsMenuMargin + addAccountHorizontalOffset
            spacing: Style.userLineSpacing + addAccountHorizontalOffset
            text: qsTr("Add account")
            onClicked: UserModel.addAccount()

            Accessible.role: Accessible.MenuItem
            Accessible.name: qsTr("Add new account")
            Accessible.onPressAction: addAccountButton.clicked()
        }

        MenuSeparator {}

        MenuItem {
            id: syncPauseButton
            height: Systray.anySyncFolders ? implicitHeight : 0
            font.pixelSize: Style.topLinePixelSize
            hoverEnabled: true
            enabled: Systray.anySyncFolders
            visible: Systray.anySyncFolders
            onClicked: Systray.syncIsPaused = !Systray.syncIsPaused
            Accessible.role: Accessible.MenuItem
            Accessible.name: Systray.syncIsPaused ? qsTr("Resume sync for all") : qsTr("Pause sync for all")
            Accessible.onPressAction: syncPauseButton.clicked()
        }

        MenuItem {
            id: settingsButton
            text: qsTr("Settings")
            font.pixelSize: Style.topLinePixelSize
            hoverEnabled: true
            onClicked: Systray.openSettings()
            Accessible.role: Accessible.MenuItem
            Accessible.name: text
            Accessible.onPressAction: settingsButton.clicked()
        }

        MenuItem {
            id: exitButton
            text: qsTr("Exit");
            font.pixelSize: Style.topLinePixelSize
            hoverEnabled: true
            onClicked: Systray.shutdown()
            Accessible.role: Accessible.MenuItem
            Accessible.name: text
            Accessible.onPressAction: exitButton.clicked() 
        }
    }

    RowLayout {
        id: accountControlRowLayout

        height: Style.trayWindowHeaderHeight
        width:  Style.rootWidth
        spacing: 0

        Image {
            id: currentAccountAvatar

            Layout.leftMargin: Style.trayHorizontalMargin
            verticalAlignment: Qt.AlignCenter
            cache: false
            source: (UserModel.currentUser && UserModel.currentUser.avatar !== "") ? UserModel.currentUser.avatar : "image://avatars/fallbackWhite"
            Layout.preferredHeight: Style.accountAvatarSize
            Layout.preferredWidth: Style.accountAvatarSize

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Current account avatar")

            Rectangle {
                id: currentAccountStatusIndicatorBackground
                visible: UserModel.currentUser && UserModel.currentUser.isConnected
                         && UserModel.currentUser.serverHasUserStatus
                         && UserModel.currentUser.status !== UserStatus.Invisible
                         && UserModel.currentUser.status !== UserStatus.Offline
                width: Style.accountAvatarStateIndicatorSize + Style.trayFolderStatusIndicatorSizeOffset
                height: width
                color: "white"
                anchors.bottom: currentAccountAvatar.bottom
                anchors.right: currentAccountAvatar.right
                radius: width * Style.trayFolderStatusIndicatorRadiusFactor
            }

            Image {
                id: currentAccountStatusIndicator
                visible: UserModel.currentUser && UserModel.currentUser.isConnected
                         && UserModel.currentUser.serverHasUserStatus
                         && UserModel.currentUser.status !== UserStatus.Invisible
                         && UserModel.currentUser.status !== UserStatus.Offline
                source: UserModel.currentUser ? UserModel.currentUser.statusIcon : ""
                cache: false
                x: currentAccountStatusIndicatorBackground.x + Style.trayFolderStatusIndicatorSizeOffset / 2
                y: currentAccountStatusIndicatorBackground.y + Style.trayFolderStatusIndicatorSizeOffset / 2
                sourceSize.width: Style.accountAvatarStateIndicatorSize
                sourceSize.height: Style.accountAvatarStateIndicatorSize

                Accessible.role: Accessible.Indicator
                Accessible.name: UserModel.desktopNotificationsAllowed ? qsTr("Current account status is online") : qsTr("Current account status is do not disturb")
            }
        }

        Column {
            id: accountLabels
            spacing: Style.extraExtraSmallSpacing
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            Layout.leftMargin: Style.userStatusSpacing
            Layout.fillWidth: true
            Layout.maximumWidth: parent.width

            EnforcedPlainTextLabel {
                id: currentAccountUser
                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                width: Style.currentAccountLabelWidth
                color: Style.currentUserHeaderTextColor
                text: UserModel.currentUser ? UserModel.currentUser.name : ""
                elide: Text.ElideRight

                font.pixelSize: Style.topLinePixelSize
                font.bold: true
            }

            EnforcedPlainTextLabel {
                id: currentAccountServer
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                width: Style.currentAccountLabelWidth
                color: Style.currentUserHeaderTextColor
                text: UserModel.currentUser ? UserModel.currentUser.server : ""
                font.pixelSize: Style.subLinePixelSize
                elide: Text.ElideRight
                visible: UserModel.numUsers() > 1
            }

            RowLayout {
                id: currentUserStatus
                visible: UserModel.currentUser && UserModel.currentUser.isConnected &&
                         UserModel.currentUser.serverHasUserStatus
                width: parent.width

                EnforcedPlainTextLabel {
                    id: emoji
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    visible: UserModel.currentUser && UserModel.currentUser.statusEmoji !== ""
                    color: Style.currentUserHeaderTextColor
                    text: UserModel.currentUser ? UserModel.currentUser.statusEmoji : ""
                    font.pixelSize: Style.subLinePixelSize
                }
                EnforcedPlainTextLabel {
                    id: message
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    Layout.fillWidth: true
                    visible: UserModel.currentUser && UserModel.currentUser.statusMessage !== ""
                    color: Style.currentUserHeaderTextColor
                    text: UserModel.currentUser && UserModel.currentUser.statusMessage !== ""
                          ? UserModel.currentUser.statusMessage
                          : UserModel.currentUser ? UserModel.currentUser.server : ""
                    font.pixelSize: Style.subLinePixelSize
                    elide: Text.ElideRight
                }
            }
        }

        Loader {
            active: root.indicator === null
            sourceComponent: Image {
                Layout.alignment: Qt.AlignRight
                verticalAlignment: Qt.AlignCenter
                horizontalAlignment: Qt.AlignRight
                Layout.leftMargin: Style.accountDropDownCaretMargin
                source:  "image://svgimage-custom-color/caret-down.svg/" + palette.windowText
                sourceSize.width: Style.accountDropDownCaretSize
                sourceSize.height: Style.accountDropDownCaretSize
                Accessible.role: Accessible.PopupMenu
                Accessible.name: qsTr("Account switcher and settings menu")
            }
        }
    }
}

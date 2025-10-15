/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

// Custom qml modules are in /theme (and included by resources.qrc)
import Style
import com.nextcloud.desktopclient

AbstractButton {
    id: userLine

    signal showUserStatusSelector(int id)
    signal showUserStatusMessageSelector(int id)


    Accessible.role: Accessible.MenuItem
    Accessible.name: qsTr("Switch to account") + " " + model.name

    height: Style.trayWindowHeaderHeight

    contentItem: RowLayout {
        id: userLineLayout
        spacing: Style.userLineSpacing
        width: parent.width

        Image {
            id: accountAvatar
            Layout.leftMargin: Style.accountIconsMenuMargin
            verticalAlignment: Qt.AlignCenter
            cache: false
            source: model.avatar !== "" ? model.avatar : Style.darkMode ? "image://avatars/fallbackWhite" : "image://avatars/fallbackBlack"
            Layout.preferredHeight: Style.accountAvatarSize
            Layout.preferredWidth: Style.accountAvatarSize

            Rectangle {
                id: accountStatusIndicatorBackground
                visible: model.isConnected && model.serverHasUserStatus
                width: accountStatusIndicator.sourceSize.width + Style.trayFolderStatusIndicatorSizeOffset
                height: width
                color: "white"
                anchors.bottom: accountAvatar.bottom
                anchors.right: accountAvatar.right
                radius: width * Style.trayFolderStatusIndicatorRadiusFactor
            }

            Image {
                id: accountStatusIndicator
                visible: model.isConnected && model.serverHasUserStatus
                source: model.statusIcon
                cache: false
                x: accountStatusIndicatorBackground.x + Style.trayFolderStatusIndicatorSizeOffset / 2
                y: accountStatusIndicatorBackground.y + Style.trayFolderStatusIndicatorSizeOffset / 2
                sourceSize.width: Style.accountAvatarStateIndicatorSize
                sourceSize.height: Style.accountAvatarStateIndicatorSize

                Accessible.role: Accessible.Indicator
                Accessible.name: model.desktopNotificationsAllowed ? qsTr("Current account status is online") : qsTr("Current account status is do not disturb")
            }
        }

        ColumnLayout {
            id: accountLabels
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Style.extraExtraSmallSpacing

            EnforcedPlainTextLabel {
                id: accountUser
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                verticalAlignment: Text.AlignBottom
                text: name
                elide: Text.ElideRight
                font.pixelSize: Style.topLinePixelSize
                font.bold: true

                color: !userLine.parent.enabled
                    ? userLine.parent.palette.mid
                    : (userLine.parent.highlighted || userLine.parent.down
                        ? Style.contrastingColor( parent.palette.highlight )
                        : userLine.parent.palette.text)
            }

            RowLayout {
                id: statusLayout
                Layout.fillWidth: true
                height: visible ? implicitHeight : 0
                visible: model.isConnected &&
                         model.serverHasUserStatus &&
                         (model.statusEmoji !== "" || model.statusMessage !== "")

                EnforcedPlainTextLabel {
                    id: emoji
                    visible: model.statusEmoji !== ""
                    text: statusEmoji

                    color: !userLine.parent.enabled
                        ? userLine.parent.palette.mid
                        : ((userLine.parent.highlighted || userLine.parent.down) && Qt.platform.os !== "windows"
                            ? Style.contrastingColor( parent.palette.highlight )
                            : userLine.parent.palette.text)
                }

                EnforcedPlainTextLabel {
                    id: message
                    Layout.fillWidth: true
                    visible: model.statusMessage !== ""
                    text: statusMessage
                    elide: Text.ElideRight
                    font.pixelSize: Style.subLinePixelSize

                    color: !userLine.parent.enabled
                        ? userLine.parent.palette.mid
                        : ((userLine.parent.highlighted || userLine.parent.down) && Qt.platform.os !== "windows"
                            ? userLine.parent.palette.highlightedText
                            : userLine.parent.palette.text)
                }
            }

            EnforcedPlainTextLabel {
                id: accountServer
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                verticalAlignment: Text.AlignTop
                text: server
                elide: Text.ElideRight
                font.pixelSize: Style.subLinePixelSize

                color: !userLine.parent.enabled
                    ? userLine.parent.palette.mid
                    : ((userLine.parent.highlighted || userLine.parent.down) && Qt.platform.os !== "windows"
                        ? Style.contrastingColor( parent.palette.highlight )
                        : userLine.parent.palette.text)
            }
        }

        Item { // Spacer
            Layout.fillWidth: true
        }

        Button {
            id: userMoreButton
            Layout.preferredWidth: Style.headerButtonIconSize
            Layout.fillHeight: true
            flat: true

            Accessible.role: Accessible.ButtonMenu
            Accessible.name: qsTr("Account actions")
            Accessible.onPressAction: userMoreButtonMouseArea.clicked()

            onClicked: userMoreButtonMenu.visible ? userMoreButtonMenu.close() : userMoreButtonMenu.popup()

            property var iconColor: !userLine.parent.enabled
                ? userLine.parent.palette.mid
                : (!hovered && ((userLine.parent.highlighted || userLine.parent.down) && Qt.platform.os !== "windows")
                    ? Style.contrastingColor( parent.palette.highlight )
                    : userLine.parent.palette.text)
            icon.source: "image://svgimage-custom-color/more.svg/" + iconColor

            AutoSizingMenu {
                id: userMoreButtonMenu
                closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                MenuItem {
                    visible: model.isConnected && model.serverHasUserStatus
                    height: visible ? implicitHeight : 0
                    text: qsTr("Set status")
                    font.pixelSize: Style.topLinePixelSize
                    hoverEnabled: true
                    onClicked: showUserStatusSelector(index)
               }

                MenuItem {
                    visible: model.isConnected && model.serverHasUserStatus
                    height: visible ? implicitHeight : 0
                    text: qsTr("Status message")
                    font.pixelSize: Style.topLinePixelSize
                    hoverEnabled: true
                    onClicked: showUserStatusMessageSelector(index)
               }

                MenuItem {
                    text: model.isConnected ? qsTr("Log out") : qsTr("Log in")
                    visible: model.canLogout
                    height: visible ? implicitHeight : 0
                    width: parent.width
                    font.pixelSize: Style.topLinePixelSize
                    hoverEnabled: true
                    onClicked: {
                        model.isConnected ? UserModel.logout(index) : UserModel.login(index)
                        accountMenu.close()
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: model.isConnected ? qsTr("Log out") : qsTr("Log in")

                    onPressed: {
                        if (model.isConnected) {
                            UserModel.logout(index)
                        } else {
                            UserModel.login(index)
                        }
                        accountMenu.close()
                    }
               }

                MenuItem {
                    id: removeAccountButton
                    text: model.removeAccountText
                    font.pixelSize: Style.topLinePixelSize
                    hoverEnabled: true
                    onClicked: {
                        UserModel.removeAccount(index)
                        accountMenu.close()
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.onPressAction: removeAccountButton.clicked()
               }
            }
        }
    }
}   // MenuItem userLine


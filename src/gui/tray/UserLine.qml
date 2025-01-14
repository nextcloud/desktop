/*
 * Copyright (C) 2019 by Dominique Fuchs <32204802+DominiqueFuchs@users.noreply.github.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

// Custom qml modules are in /theme (and included by resources.qrc)
import Style
import com.ionos.hidrivenext.desktopclient

AbstractButton {
    id: userLine

    property bool isHovered: userLine.hovered || userLine.visualFocus
    property bool isActive: userLine.pressed

    signal showUserStatusSelector(int id)


    Accessible.role: Accessible.MenuItem
    Accessible.name: qsTr("Switch to account") + " " + model.name

    height: Style.sesAccountMenuHeight

    leftPadding: Style.sesMediumMargin
    topPadding: Style.sesSmallMargin
    bottomPadding: Style.sesSmallMargin

    background: Rectangle {
        radius: 0
        anchors.fill: parent
        anchors.margins: 1
        color: userLine.isHovered && !userMoreButton.isHovered ? Style.sesAccountMenuHover : "transparent"
    }

    contentItem: RowLayout {
        id: userLineLayout
        spacing: Style.sesSmallMargin

        Image {
            id: accountAvatar
            verticalAlignment: Qt.AlignCenter
            cache: false
            source: Style.sesAvatar
        }

        ColumnLayout {
            id: accountLabels
            Layout.fillWidth: true
            Layout.fillHeight: true

            EnforcedPlainTextLabel {
                id: accountUser
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                verticalAlignment: Text.AlignBottom
                text: name
                elide: Text.ElideRight
                font: root.font
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
                    topPadding: -Style.accountLabelsSpacing
                }

                EnforcedPlainTextLabel {
                    id: message
                    Layout.fillWidth: true
                    visible: model.statusMessage !== ""
                    text: statusMessage
                    elide: Text.ElideRight
                    font: root.font
                    leftPadding: Style.accountLabelsSpacing
                }
            }
        }

        Button {
            id: userMoreButton
            Layout.preferredWidth: Style.headerButtonIconSize
            Layout.preferredHeight: Layout.preferredWidth
            Layout.rightMargin: Style.sesMediumMargin
            flat: true

            property bool isHovered: userMoreButton.hovered || userMoreButton.visualFocus
            property bool isActive: userMoreButton.pressed || userMoreButtonMenu.visible

            icon.source: "qrc:///client/theme/more.svg"
            icon.color: userMoreButton.isActive || userMoreButton.isHovered ? Style.sesWhite : Style.sesIconDarkColor

            Accessible.role: Accessible.ButtonMenu
            Accessible.name: qsTr("Account actions")
            Accessible.onPressAction: userMoreButtonMouseArea.clicked()

            onClicked: userMoreButtonMenu.visible ? userMoreButtonMenu.close() : userMoreButtonMenu.popup()
            background: Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                color: userMoreButton.isActive ? Style.sesActionPressed : userMoreButton.isHovered ? Style.sesActionHover : "transparent"
                radius: width / 2
            }

            AutoSizingMenu {
                id: userMoreButtonMenu
                closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                background: Rectangle {
                    radius: Style.sesCornerRadius
                    border.color: Style.sesBorderColor
                }

                MenuItem {
                    visible: false
                    height: visible ? implicitHeight : 0
                    text: qsTr("Set status")
                    font: root.font
                    palette.windowText: Style.ncTextColor
                    hoverEnabled: true
                    onClicked: showUserStatusSelector(index)
               }

                MenuItem {
                    id: logInOutButton

                    property bool isHovered: logInOutButton.hovered || logInOutButton.visualFocus
                    property bool isActive: logInOutButton.pressed

                    icon.source: Style.sesLogout
                    icon.color: Style.sesIconDarkColor
                    leftPadding: Style.sesMediumMargin
                    topPadding: Style.sesAccountMenuItemPadding
                    bottomPadding: Style.sesAccountMenuItemPadding
                    spacing: Style.sesSmallMargin
                    text: model.isConnected ? qsTr("Log out") : qsTr("Log in")
                    font: root.font
                    palette.windowText: Style.ncTextColor
                    hoverEnabled: true
                    onClicked: {
                        model.isConnected ? UserModel.logout(index) : UserModel.login(index)
                        accountMenu.close()
                    }

                    background: Item {
                        height: parent.height
                        width: parent.menu.width
                        Rectangle {
                            radius: 0
                            anchors.fill: parent
                            anchors.margins: 1
                            color: logInOutButton.isActive ? Style.sesButtonPressed :
                                   logInOutButton.isHovered ? Style.sesAccountMenuHover : "transparent"
                        }
                    }

                    Accessible.role: Accessible.MenuItem
                    Accessible.name: model.isConnected ? qsTr("Log out") : qsTr("Log in")
                }

                MenuItem {

                    property bool isHovered: removeAccountButton.hovered || removeAccountButton.visualFocus
                    property bool isActive: removeAccountButton.pressed

                    id: removeAccountButton
                    icon.source: Style.sesDelete
                    icon.color: Style.sesIconDarkColor
                    leftPadding: Style.sesMediumMargin
                    topPadding: Style.sesAccountMenuItemPadding
                    bottomPadding: Style.sesAccountMenuItemPadding
                    spacing: Style.sesSmallMargin
                    text: qsTr("Remove account")
                    font: root.font
                    palette.windowText: Style.ncTextColor
                    hoverEnabled: true
                    onClicked: {
                        UserModel.removeAccount(index)
                        accountMenu.close()
                    }

                    background: Item {
                        height: parent.height
                        width: parent.menu.width
                        Rectangle {
                            radius: 0
                            anchors.fill: parent
                            anchors.margins: 1
                            color: removeAccountButton.isActive ? Style.sesButtonPressed :
                                   removeAccountButton.isHovered ? Style.sesAccountMenuHover : "transparent"
                        }
                    }

                    Accessible.role: Accessible.MenuItem
                    Accessible.name: text
                    Accessible.onPressAction: removeAccountButton.clicked()
               }
            }
        }
    }
}   // MenuItem userLine
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
import com.ionos.hidrivenext.desktopclient

AbstractButton {
    id: userLine

    property bool isHovered: userLine.hovered || userLine.visualFocus
    property bool isActive: userLine.pressed

    signal showUserStatusSelector(int id)
    signal showUserStatusMessageSelector(int id)

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
            spacing: Style.extraExtraSmallSpacing

            EnforcedPlainTextLabel {
                id: accountUser
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                verticalAlignment: Text.AlignBottom
                text: name
                elide: Text.ElideRight
                font: userLine.font
                color: Style.sesTrayFontColor
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
                            ? userLine.parent.palette.highlightedText
                            : userLine.parent.palette.text)
                }

                EnforcedPlainTextLabel {
                    id: message
                    Layout.fillWidth: true
                    visible: model.statusMessage !== ""
                    text: statusMessage
                    elide: Text.ElideRight
                    font: userLine.font
                    leftPadding: Style.accountLabelsSpacing
                    font.pixelSize: Style.subLinePixelSize

                    color: !userLine.parent.enabled
                        ? userLine.parent.palette.mid
                        : ((userLine.parent.highlighted || userLine.parent.down) && Qt.platform.os !== "windows"
                            ? userLine.parent.palette.highlightedText
                            : userLine.parent.palette.text)
                }
            }
        }

        IconButton {
            id: userMoreButton
            Layout.preferredWidth: Style.headerButtonIconSize
            Layout.preferredHeight: Layout.preferredWidth
            Layout.rightMargin: Style.sesMediumMargin
            flat: true

            property bool isHovered: userMoreButton.hovered || userMoreButton.visualFocus
            property bool isActive: userMoreButton.pressed || userMoreButtonMenu.visible

            iconSource: Style.sesMore
            iconSourceHovered: Style.sesMoreHover

            Accessible.role: Accessible.ButtonMenu
            Accessible.name: qsTr("Account actions")
            Accessible.onPressAction: userMoreButtonMouseArea.clicked()

            onClicked: userMoreButtonMenu.visible ? userMoreButtonMenu.close() : userMoreButtonMenu.popup()

            Menu {
                id: userMoreButtonMenu
                width: Style.sesAccountMenuWidth
                height: Math.min(implicitHeight, maxMenuHeight)
                closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape
                height: implicitHeight

                bottomInset: 0
                topInset: 0
                rightInset: 0
                leftInset: 0
                rightPadding: 0
                leftPadding: 0
                topPadding: 0
                bottomPadding: 0

                background: Rectangle {
                    radius: Style.sesCornerRadius
                    border.color: Style.sesBorderColor
                }

                MenuItem {
                    id: logInOutButton

                    property bool isHovered: logInOutButton.hovered || logInOutButton.visualFocus
                    property bool isActive: logInOutButton.pressed

                    palette.text: Style.sesTrayFontColor

                    Component.onCompleted: {
                        if (contentItem && contentItem.hasOwnProperty("color")) {
                            contentItem.color = logInOutButton.palette.text
                        }
                    }
                    icon.source: Style.sesLogout
                    icon.color: Style.sesIconDarkColor
                    leftPadding: Style.sesMediumMargin
                    topPadding: Style.sesAccountMenuItemPadding
                    bottomPadding: Style.sesAccountMenuItemPadding
                    spacing: Style.sesSmallMargin
                    text: model.isConnected ? qsTr("Log out") : qsTr("Log in")
                    font: userLine.font
                    palette.windowText: Style.ncTextColor
                    hoverEnabled: true

                    onClicked: {
                        model.isConnected ? UserModel.logout(index) : UserModel.login(index)
                        accountMenu.close()
                    }
                    // TODO SES-459 Check Merge "setStatus" && "onPressed"
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

                    property bool isHovered: removeAccountButton.hovered || removeAccountButton.visualFocus
                    property bool isActive: removeAccountButton.pressed

                    palette.text: Style.sesTrayFontColor

                    Component.onCompleted: {
                        if (contentItem && contentItem.hasOwnProperty("color")) {
                            contentItem.color = removeAccountButton.palette.text
                        }
                    }
                    icon.source: Style.sesDelete
                    icon.color: Style.sesIconDarkColor
                    leftPadding: Style.sesMediumMargin
                    topPadding: Style.sesAccountMenuItemPadding
                    bottomPadding: Style.sesAccountMenuItemPadding
                    spacing: Style.sesSmallMargin
                    text: qsTr("Remove account")
                    font: userLine.font
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

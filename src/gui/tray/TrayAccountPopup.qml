/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Style
import com.nextcloud.desktopclient

// Keep behavior and layout aligned with src/gui/macOS/trayaccountpopup_mac.mm.

Window {
    id: root

    readonly property bool hasAccounts: UserModel && UserModel.count > 0
    readonly property color rowHoverColor: Style.darkMode
                                               ? Qt.rgba(1, 1, 1, Style.trayAccountPopupRowHoverOpacity)
                                               : Qt.rgba(0, 0, 0, Style.trayAccountPopupRowHoverOpacity)

    width: Style.trayAccountPopupWidth
    height: contentColumn.height
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    property bool _closing: false
    property bool _hadFocusSinceShow: false

    onVisibleChanged: {
        if (visible) {
            _hadFocusSinceShow = false
        }
    }

    onActiveChanged: {
        if (active) {
            _hadFocusSinceShow = true
        } else if (_hadFocusSinceShow && !_closing) {
            Systray.hideWindow()
        }
        _closing = false
    }

    Rectangle {
        id: popupContainer
        anchors.fill: parent
        radius: Style.trayWindowRadius
        color: palette.window
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        clip: true
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: popupContainer.width
                height: popupContainer.height
                radius: popupContainer.radius
                visible: false
            }
        }

        Column {
            id: contentColumn
            width: parent.width
            spacing: 0

            Item {
                width: parent.width
                height: Style.trayAccountPopupTopPadding
            }

            Repeater {
                model: UserModel

                delegate: ItemDelegate {
                    id: accountRow
                    width: root.width
                    height: Style.trayAccountPopupRowHeight
                    hoverEnabled: true
                    topInset: 0
                    leftInset: 0
                    rightInset: 0
                    bottomInset: 0
                    padding: 0
                    leftPadding: Style.trayAccountPopupRowPadding
                    rightPadding: Style.trayAccountPopupRowPadding

                    background: Item {
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: Style.trayAccountPopupHoverMargin
                            anchors.rightMargin: Style.trayAccountPopupHoverMargin
                            anchors.topMargin: Style.trayAccountPopupAccountHoverVerticalMargin
                            anchors.bottomMargin: Style.trayAccountPopupAccountHoverVerticalMargin
                            radius: Style.trayAccountPopupHoverRadius
                            color: accountRow.hovered ? root.rowHoverColor : "transparent"
                            Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                        }
                    }

                    contentItem: RowLayout {
                        spacing: Style.trayAccountPopupRowSpacing

                        Image {
                            Layout.preferredWidth: Style.trayAccountPopupAvatarSize
                            Layout.preferredHeight: Style.trayAccountPopupAvatarSize
                            source: model.avatar !== "" ? model.avatar
                                : (Style.darkMode ? "image://avatars/fallbackWhite" : "image://avatars/fallbackBlack")
                            fillMode: Image.PreserveAspectCrop
                            cache: false
                            layer.enabled: true
                            layer.effect: OpacityMask {
                                maskSource: Rectangle {
                                    width: Style.trayAccountPopupAvatarSize
                                    height: Style.trayAccountPopupAvatarSize
                                    radius: width / 2
                                    visible: false
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            EnforcedPlainTextLabel {
                                Layout.fillWidth: true
                                text: model.name
                                font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                color: palette.windowText
                            }

                            EnforcedPlainTextLabel {
                                Layout.fillWidth: true
                                text: model.server
                                font.pixelSize: Style.trayAccountPopupSecondaryFontSize
                                elide: Text.ElideRight
                                color: palette.windowText
                                opacity: 0.6
                            }
                        }

                        Image {
                            Layout.preferredWidth: Style.trayAccountPopupSyncIconSize
                            Layout.preferredHeight: Style.trayAccountPopupSyncIconSize
                            source: model.syncStatusIcon
                            sourceSize: Qt.size(Style.trayAccountPopupSyncIconSize,
                                                Style.trayAccountPopupSyncIconSize)
                        }

                        EnforcedPlainTextLabel {
                            text: "›"
                            font.pixelSize: Style.trayAccountPopupChevronFontSize
                            color: palette.windowText
                            opacity: 0.35
                        }
                    }

                    onClicked: {
                        root._closing = true
                        UserModel.currentUserId = model.id
                        Systray.showQMLWindow()
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: Style.trayWindowBorderWidth
                color: palette.dark
            }

            Item {
                width: parent.width
                height: Style.trayAccountPopupActionVerticalPadding
            }

            ItemDelegate {
                id: addAccountRow
                visible: Systray.enableAddAccount
                width: root.width
                height: visible ? Style.trayAccountPopupActionHeight : 0
                hoverEnabled: true
                topInset: 0
                leftInset: 0
                rightInset: 0
                bottomInset: 0
                padding: 0
                leftPadding: Style.trayAccountPopupRowPadding

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Style.trayAccountPopupHoverMargin
                        anchors.rightMargin: Style.trayAccountPopupHoverMargin
                        radius: Style.trayAccountPopupHoverRadius
                        color: addAccountRow.hovered ? root.rowHoverColor : "transparent"
                        Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                    }
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Add account")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    root._closing = true
                    Systray.hideWindow()
                    Systray.openAccountWizard()
                }
            }

            ItemDelegate {
                id: settingsRow
                width: root.width
                height: Style.trayAccountPopupActionHeight
                hoverEnabled: true
                topInset: 0
                leftInset: 0
                rightInset: 0
                bottomInset: 0
                padding: 0
                leftPadding: Style.trayAccountPopupRowPadding

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Style.trayAccountPopupHoverMargin
                        anchors.rightMargin: Style.trayAccountPopupHoverMargin
                        radius: Style.trayAccountPopupHoverRadius
                        color: settingsRow.hovered ? root.rowHoverColor : "transparent"
                        Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                    }
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Settings")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    root._closing = true
                    Systray.hideWindow()
                    Systray.openSettings()
                }
            }

            ItemDelegate {
                id: quitRow
                width: root.width
                height: Style.trayAccountPopupActionHeight
                hoverEnabled: true
                topInset: 0
                leftInset: 0
                rightInset: 0
                bottomInset: 0
                padding: 0
                leftPadding: Style.trayAccountPopupRowPadding

                background: Item {
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Style.trayAccountPopupHoverMargin
                        anchors.rightMargin: Style.trayAccountPopupHoverMargin
                        radius: Style.trayAccountPopupHoverRadius
                        color: quitRow.hovered ? root.rowHoverColor : "transparent"
                        Behavior on color { ColorAnimation { duration: Style.trayAccountPopupHoverAnimationDuration } }
                    }
                }

                contentItem: EnforcedPlainTextLabel {
                    text: qsTr("Quit")
                    font.pixelSize: Style.trayAccountPopupPrimaryFontSize
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    root._closing = true
                    Systray.shutdown()
                }
            }

            Item {
                width: parent.width
                height: Style.trayAccountPopupActionVerticalPadding
            }
        }
    }
}

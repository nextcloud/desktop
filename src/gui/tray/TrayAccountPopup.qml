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

Window {
    id: root

    readonly property int popupWidth: 340
    readonly property int rowHeight: 56
    readonly property int actionHeight: 36
    readonly property int avatarSize: 34

    width: popupWidth
    height: contentColumn.height
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    property bool _closing: false

    onActiveChanged: {
        if (!active && !_closing) {
            Systray.hideWindow()
        }
        _closing = false
    }

    Rectangle {
        anchors.fill: parent
        radius: Style.trayWindowRadius
        color: palette.window
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark

        Column {
            id: contentColumn
            width: parent.width

            Repeater {
                model: UserModel

                delegate: ItemDelegate {
                    id: accountRow
                    width: root.popupWidth
                    height: root.rowHeight
                    hoverEnabled: true
                    padding: 0
                    leftPadding: 12
                    rightPadding: 12

                    background: Rectangle {
                        color: accountRow.hovered
                            ? Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.07)
                            : "transparent"
                        Behavior on color { ColorAnimation { duration: 80 } }
                    }

                    contentItem: RowLayout {
                        spacing: 10

                        Image {
                            Layout.preferredWidth: root.avatarSize
                            Layout.preferredHeight: root.avatarSize
                            source: model.avatar !== "" ? model.avatar
                                : (Style.darkMode ? "image://avatars/fallbackWhite" : "image://avatars/fallbackBlack")
                            fillMode: Image.PreserveAspectCrop
                            cache: false
                            layer.enabled: true
                            layer.effect: OpacityMask {
                                maskSource: Rectangle {
                                    width: root.avatarSize
                                    height: root.avatarSize
                                    radius: width / 2
                                    visible: false
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Label {
                                Layout.fillWidth: true
                                text: model.name
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                color: palette.windowText
                            }

                            Label {
                                Layout.fillWidth: true
                                text: model.server
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                color: palette.windowText
                                opacity: 0.6
                            }
                        }

                        Image {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            source: model.syncStatusIcon
                            sourceSize: Qt.size(16, 16)
                        }

                        Label {
                            text: "›"
                            font.pixelSize: 18
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

            ItemDelegate {
                id: settingsRow
                width: root.popupWidth
                height: root.actionHeight
                hoverEnabled: true
                padding: 0
                leftPadding: 12

                background: Rectangle {
                    color: settingsRow.hovered
                        ? Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.07)
                        : "transparent"
                    Behavior on color { ColorAnimation { duration: 80 } }
                }

                contentItem: Label {
                    text: qsTr("Settings")
                    font.pixelSize: 13
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
                width: root.popupWidth
                height: root.actionHeight
                hoverEnabled: true
                padding: 0
                leftPadding: 12

                background: Rectangle {
                    color: quitRow.hovered
                        ? Qt.rgba(root.palette.windowText.r, root.palette.windowText.g, root.palette.windowText.b, 0.07)
                        : "transparent"
                    Behavior on color { ColorAnimation { duration: 80 } }
                }

                contentItem: Label {
                    text: qsTr("Quit")
                    font.pixelSize: 13
                    color: palette.windowText
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    root._closing = true
                    Systray.shutdown()
                }
            }
        }
    }
}

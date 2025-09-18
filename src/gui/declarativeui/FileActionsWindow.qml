/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import com.nextcloud.desktopclient
import Style

ApplicationWindow {
    id: root
    height: Style.trayWindowWidth
    width: Systray.useNormalWindow ? Style.trayWindowHeight : Style.trayWindowWidth
    flags: Systray.useNormalWindow ? Qt.Window : Qt.Dialog | Qt.FramelessWindowHint
    visible: true

    property var accountState: ({})
    property string localPath: ""
    property string shortLocalPath: ""
    property var response: ({})

    title: qsTr("File actions for %1").arg(root.shortLocalPath)

    FileActionsModel {
        id: fileActionModel
        accountState: root.accountState
        localPath: root.localPath
    }

    background: Rectangle {
        //radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        color: palette.window
    }

    // TO FIX: OpacityMask {
    //     anchors.fill: parent
    //     anchors.margins: Style.trayWindowBorderWidth
    //     source: ShaderEffectSource {
    //         sourceItem: windowContent
    //         hideSource: true
    //     }
    //     maskSource: Rectangle {
    //         width: root.width
    //         height: root.height
    //         radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
    //     }
    // }

    ColumnLayout {
        id: windowContent
        anchors.fill: parent
        anchors.margins:  Style.standardSpacing

        RowLayout {
            id: windowHeader
            Layout.fillWidth: true
            spacing: Style.standardSpacing

            Image {
                source: "image://svgimage-custom-color/file-open.svg/" + palette.windowText
                width: Style.minimumActivityItemHeight
                height: Style.minimumActivityItemHeight
                Layout.alignment: Qt.AlignVCenter
                Layout.margins: Style.extraSmallSpacing
            }

            Label {
                id: headerLocalPath
                text: root.shortLocalPath
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: Style.pixelSize
                color: palette.text
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
            }

            Button {
                id: closeButton
                flat: true
                padding: 0
                spacing: 0
                icon.source: "image://svgimage-custom-color/close.svg/" + palette.windowText
                icon.width: Style.extraSmallIconSize
                icon.height: Style.extraSmallIconSize
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                Layout.rightMargin: Style.extraSmallSpacing
                Layout.topMargin: Style.extraSmallSpacing
                onClicked: root.close()
                background: Rectangle {
                    color: "transparent"
                    radius: 0
                    border.width: closeButton.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.fill: parent
                    Layout.margins: Style.extraSmallSpacing
                }
            }
        }

        Rectangle {
            id: lineTop
            Layout.fillWidth: true
            height: Style.extraExtraSmallSpacing
            color: palette.dark
        }

        ListView {
            id: fileActionsView
            model: fileActionModel
            clip: true
            spacing: Style.trayHorizontalMargin
            Layout.fillWidth: true
            Layout.fillHeight: true
            delegate: fileActionsDelegate
        }

        Button {
            id: responseButton
            visible: responseText.text !== ""
            flat: true
            Layout.fillWidth: true
            implicitHeight: Style.activityListButtonHeight

            padding: 0
            leftPadding: Style.smallSpacing
            rightPadding: Style.smallSpacing
            spacing: Style.standardSpacing

            background: Rectangle {
                //radius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius
                border.width: Style.trayWindowBorderWidth
                border.color: palette.dark
                color: palette.window
            }

            contentItem: Row {
                id: responseContent
                anchors.fill: parent
                anchors.margins: Style.smallSpacing
                spacing: Style.halfTrayWindowRadius
                Layout.fillWidth: true

                Image {
                    source: "image://svgimage-custom-color/backup.svg/" + palette.windowText
                    width: Style.accountAvatarStateIndicatorSize
                    height: Style.accountAvatarStateIndicatorSize
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: responseText
                    text: fileActionModel.responseLabel
                    textFormat: Text.RichText
                    color: palette.text
                    font.pointSize: Style.pixelSize
                    font.underline: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: responseArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally(fileActionModel.responseUrl)
            }
        }
    }

    Component {
        id: fileActionsDelegate

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Style.extraSmallSpacing
            spacing: Style.standardSpacing
            height: implicitHeight
            width: implicitWidth

            required property string name
            required property int index
            required property string icon

            Button {
                id: fileActionButton
                flat: true
                Layout.fillWidth: true
                implicitHeight: Style.activityListButtonHeight

                padding: 0
                leftPadding: Style.smallSpacing
                rightPadding: Style.smallSpacing
                spacing: Style.standardSpacing

                contentItem: Row {
                    id: fileActionsContent
                    anchors.fill: parent
                    anchors.margins: Style.smallSpacing
                    spacing: Style.standardSpacing
                    Layout.fillWidth: true

                    Image {
                        source: icon + palette.windowText
                        width: Style.activityListButtonHeight
                        height: Style.activityListButtonHeight
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        text: name
                        color: palette.text
                        font.pixelSize: Style.defaultFontPtSize
                        verticalAlignment: Text.AlignVCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: fileActionMouseArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: fileActionModel.createRequest(index)
                }
            }
        }
    }
}

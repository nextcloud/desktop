/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import com.nextcloud.desktopclient
import Style
import "./../tray"

ApplicationWindow {
    id: root
    height: Style.filesActionsHeight
    width: Style.filesActionsWidth
    flags: Systray.useNormalWindow ? Qt.Window : Qt.Dialog | Qt.FramelessWindowHint
    visible: true
    color: "transparent"

    property var accountState: ({})
    property string localPath: ""
    property string shortLocalPath: ""
    property string fileId: ""
    property string remoteItemPath: ""

    readonly property int windowRadius: Systray.useNormalWindow ? 0.0 : Style.trayWindowRadius

    title: qsTr("File actions for %1").arg(root.shortLocalPath)

    FileActionsModel {
        id: fileActionModel
        accountState: root.accountState
        localPath: root.localPath
        fileId: root.fileId
        remoteItemPath: root.remoteItemPath
    }

    background: Rectangle {
        id: maskSource
        radius: root.windowRadius
        border.width: Style.trayWindowBorderWidth
        border.color: palette.dark
        color: palette.window
    }

    OpacityMask {
        anchors.fill: parent
        anchors.margins: Style.trayWindowBorderWidth
        source: maskSourceItem
        maskSource: maskSource
    }

    Rectangle {
        id: maskSourceItem
        anchors.fill: parent
        anchors.margins: Style.standardSpacing
        radius: root.windowRadius
        clip: true
        color: Style.colorWithoutTransparency(palette.base)

        ColumnLayout {
            id: windowContent
            anchors.fill: parent
            anchors.margins: Style.standardSpacing

            RowLayout {
                id: windowHeader
                Layout.fillWidth: true
                spacing: Style.standardSpacing

                Image {
                    source: fileActionModel.fileIcon + palette.windowText
                    Layout.preferredWidth: Style.minimumActivityItemHeight
                    Layout.preferredHeight: Style.minimumActivityItemHeight
                    Layout.alignment: Qt.AlignVCenter
                    Layout.margins: Style.extraSmallSpacing
                }

                EnforcedPlainTextLabel {
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
                    padding: Style.extraSmallSpacing
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
                        radius: root.windowRadius
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
                Layout.minimumHeight: Style.extraExtraSmallSpacing
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
                implicitHeight: responseContent.implicitHeight

                padding: Style.standardSpacing
                leftPadding: Style.standardSpacing
                rightPadding: Style.standardSpacing
                spacing: Style.standardSpacing

                background: Rectangle {
                    id: responseBorder
                    radius: root.windowRadius
                    border.width: Style.trayWindowBorderWidth
                    border.color: palette.dark
                    color: palette.window
                    Layout.fillWidth: true
                }

                contentItem: RowLayout {
                    id: responseContent
                    anchors.fill: parent
                    anchors.margins: Style.smallSpacing
                    spacing: Style.standardSpacing
                    Layout.fillWidth: true
                    Layout.minimumHeight: Style.accountAvatarStateIndicatorSize

                    Image {
                        source: "image://svgimage-custom-color/backup.svg/" + palette.windowText
                        Layout.preferredWidth: Style.minimumActivityItemHeight
                        Layout.preferredHeight: Style.minimumActivityItemHeight
                        Layout.alignment: Qt.AlignLeft
                        Layout.bottomMargin: Style.standardSpacing
                    }

                    Text {
                        id: responseText
                        text: fileActionModel.responseLabel
                        textFormat: Text.RichText
                        color: palette.text
                        font.pointSize: Style.pixelSize
                        font.underline: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        bottomPadding: Style.standardSpacing
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
    }

    Component {
        id: fileActionsDelegate

        RowLayout {
            id: fileAction
            Layout.fillWidth: true
            height: implicitHeight
            width: parent.width

            required property string name
            required property int index
            required property string icon

            Button {
                id: fileActionButton
                flat: true
                Layout.fillWidth: true
                implicitHeight: Style.activityListButtonHeight

                padding: Style.standardSpacing

                contentItem: Row {
                    id: fileActionsContent
                    anchors.fill: parent
                    anchors.topMargin: Style.standardSpacing
                    anchors.rightMargin: Style.standardSpacing
                    anchors.bottomMargin: Style.standardSpacing
                    anchors.leftMargin: Style.extraSmallSpacing
                    spacing: Style.standardSpacing
                    Layout.fillWidth: true

                    Image {
                        source: fileAction.icon + palette.windowText
                        width: Style.minimumActivityItemHeight
                        height: Style.minimumActivityItemHeight
                        fillMode: Image.PreserveAspectFit
                        Layout.preferredWidth: Style.minimumActivityItemHeight
                        Layout.preferredHeight: Style.minimumActivityItemHeight
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    EnforcedPlainTextLabel {
                        text: fileAction.name
                        color: palette.text
                        font.pixelSize: Style.defaultFontPtSize
                        verticalAlignment: Text.AlignVCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                background: Rectangle {
                    color: "transparent"
                    radius: root.windowRadius
                    border.width: fileActionButton.hovered ? Style.trayWindowBorderWidth : 0
                    border.color: palette.dark
                    anchors.margins: Style.standardSpacing
                    height: parent.height
                    width: parent.width
                }

                MouseArea {
                    id: fileActionMouseArea
                    anchors.fill: parent
                    anchors.margins: Style.standardSpacing
                    cursorShape: Qt.PointingHandCursor
                    onClicked: fileActionModel.createRequest(fileAction.index)
                }
            }
        }
    }
}

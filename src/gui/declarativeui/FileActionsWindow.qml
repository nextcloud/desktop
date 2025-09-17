/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import com.nextcloud.desktopclient
import Style

ApplicationWindow {
    id: root
    width: 400
    height: 300
    minimumWidth: 300
    minimumHeight: 200
    flags: Qt.Dialog
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

    Rectangle {
        anchors.fill: parent
        color: Style.infoBoxBackgroundColor
        //radius: Style.trayWindowRadius
        border.color: Style.accentColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins:  Style.standardSpacing
            spacing: Style.standardSpacing

            RowLayout {
                id: windowHeader
                Layout.fillWidth: true
                spacing: Style.standardSpacing

                Image {
                    source: "image://svgimage-custom-color/file-open.svg/" + palette.windowText
                    width: Style.minimumActivityItemHeight
                    height: Style.minimumActivityItemHeight
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Style.extraSmallSpacing

                    Label {
                        text: root.shortLocalPath
                        font.bold: true
                        font.pixelSize: Style.pixelSize
                        color: Style.ncHeaderTextColor
                    }
                }
            }

            Rectangle {
                id: lineTop
                Layout.fillWidth: true
                height: Style.extraExtraSmallSpacing
                color: Style.accentColor
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

            Rectangle {
                id: lineBottom
                Layout.fillWidth: true
                height: Style.extraExtraSmallSpacing
                color: Style.accentColor
            }

            Button {
                id: responseButton
                visible: responseText.text !== ""
                flat: true
                Layout.fillWidth: true
                implicitHeight: Style.activityListButtonHeight

                padding: 0
                leftPadding: Style.standardSpacing
                rightPadding: Style.standardSpacing
                spacing: Style.standardSpacing

                contentItem: Row {
                    id: responseContent
                    anchors.fill: parent
                    anchors.margins: Style.smallSpacing
                    spacing: Style.standardSpacing

                    Image {
                        source: "image://svgimage-custom-color/public.svg/" + palette.windowText
                        width: Style.minimumActivityItemHeight
                        height: Style.minimumActivityItemHeight
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: responseText
                        text: fileActionModel.responseLabel
                        textFormat: Text.RichText
                        color: Style.ncHeaderTextColor
                        font.pointSize: Style.pixelSize
                        font.underline: true
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            id: responseArea
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(fileActionModel.responseUrl)
                        }
                    }
                }

                ToolTip {
                    visible: responseButton.hovered
                    text: fileActionModel.responseLabel
                }
            }

            Rectangle {
                id: repsonseLineBottom
                visible: response.text != ""
                Layout.fillWidth: true
                height: Style.extraExtraSmallSpacing
                color: Style.accentColor
            }
        }
    }

    Component {
        id: fileActionsDelegate

        RowLayout {
            Layout.fillWidth: true
            spacing: Style.standardSpacing
            height: implicitHeight

            required property string name
            required property int index
            required property string icon

            Button {
                id: fileActionButton
                flat: true
                Layout.fillWidth: true
                implicitHeight: Style.activityListButtonHeight

                padding: 0
                leftPadding: Style.standardSpacing
                rightPadding: Style.standardSpacing
                spacing: Style.standardSpacing

                contentItem: Row {
                    id: fileActionsContent
                    anchors.fill: parent
                    anchors.margins: Style.smallSpacing
                    spacing: Style.standardSpacing

                    Image {
                        source: icon
                        width: Style.minimumActivityItemHeight
                        height: Style.minimumActivityItemHeight
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        text: name
                        color: Style.ncHeaderTextColor
                        font.pixelSize: Style.pixelSize
                        verticalAlignment: Text.AlignVCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                ToolTip {
                    visible: fileActionButton.hovered
                    text: name
                }

                onClicked: fileActionModel.createRequest(index)
            }
        }
    }
}

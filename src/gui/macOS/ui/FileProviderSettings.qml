/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0
import "../../filedetails"
import "../../tray"

import com.nextcloud.desktopclient 1.0

Page {
    id: root

    property bool showBorder: false
    property var controller: FileProviderSettingsController
    property string accountUserIdAtHost: ""

    title: qsTr("Virtual files settings")

    background: Rectangle {
        color: palette.light
        border.width: root.showBorder ? Style.normalBorderWidth : 0
        border.color: palette.mid
    }

    leftPadding: 0
    rightPadding: 0
    topPadding: 12 // Style.standardSpacing is 10, the QtWidgets layout uses 12.  set it here as well to avoid a rough cutoff
    bottomPadding: 12
    implicitHeight: rootColumn.implicitHeight + topPadding + bottomPadding

    ColumnLayout {
        id: rootColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Style.standardSpacing

        RowLayout {
            id: virtualFilesLayout

            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(0, virtualFilesLayout.width - vfsEnabledCheckBox.width - virtualFilesLayout.spacing)
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.WordWrap
                text: qsTr("Virtual files appear like regular files, but they do not use local storage space. The content downloads automatically when you open the file. Virtual files and classic sync can not be used at the same time.")
            }
            Switch {
                id: vfsEnabledCheckBox
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.preferredWidth: 30
                Layout.preferredHeight: 16
                implicitWidth: 30
                implicitHeight: 16
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
                onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)

                indicator: Rectangle {
                    readonly property color enabledTrackColor: vfsEnabledCheckBox.checked ? "#00679e" : "#6b6b6b"
                    readonly property real disabledTrackAlpha: vfsEnabledCheckBox.checked ? 0.45 : 0.35

                    implicitWidth: 30
                    implicitHeight: 16
                    x: 0
                    y: parent.height / 2 - height / 2
                    radius: height / 2
                    color: Qt.rgba(enabledTrackColor.r, enabledTrackColor.g, enabledTrackColor.b, vfsEnabledCheckBox.enabled ? 1.0 : disabledTrackAlpha)

                    Rectangle {
                        width: 10
                        height: 10
                        x: vfsEnabledCheckBox.checked ? parent.width - width - 3 : 3
                        y: 3.6
                        radius: width / 2
                        color: vfsEnabledCheckBox.enabled ? "#26000000" : "#14000000"
                    }

                    Rectangle {
                        width: 10
                        height: 10
                        x: vfsEnabledCheckBox.checked ? parent.width - width - 3 : 3
                        y: 3
                        radius: width / 2
                        color: "#ffffff"
                        opacity: vfsEnabledCheckBox.enabled ? 1.0 : 0.8
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: height / 2
                        color: "transparent"
                        border.width: vfsEnabledCheckBox.activeFocus ? 2 : 0
                        border.color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.35)
                    }
                }

                contentItem: Item {}
            }
        }
    }
}

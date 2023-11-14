/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0
import "../../filedetails"
import "../../tray"

import com.nextcloud.desktopclient 1.0

Page {
    id: root

    property bool showBorder: true
    property var controller: FileProviderSettingsController
    property string accountUserIdAtHost: ""

    title: qsTr("Virtual files settings")

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally.
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        brightText: Style.ncTextBrightColor
        highlight: Style.lightHover
        highlightedText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.ncSecondaryTextColor
        mid: Style.darkerHover
        dark: Style.menuBorder
        button: Style.buttonBackgroundColor
        window: Style.backgroundColor
        base: Style.backgroundColor
        toolTipBase: Style.backgroundColor
        toolTipText: Style.ncTextColor
    }

    background: Rectangle {
        color: palette.window
        border.width: root.showBorder ? Style.normalBorderWidth : 0
        border.color: root.palette.dark
    }

    padding: Style.standardSpacing

    ColumnLayout {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            text: qsTr("General settings")
            font.bold: true
            font.pointSize: root.font.pointSize + 2
            elide: Text.ElideRight
        }

        CheckBox {
            id: vfsEnabledCheckBox
            Layout.fillWidth: true
            text: qsTr("Enable virtual files")
            checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
            onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)
        }

        GridLayout {
            id: generalActionsGrid

            property real localUsedStorage: root.controller.localStorageUsageGbForAccount(root.accountUserIdAtHost)
            property real remoteUsedStorage: root.controller.remoteStorageUsageGbForAccount(root.accountUserIdAtHost)

            Layout.fillWidth: true
            columns: 3
            visible: vfsEnabledCheckBox.checked

            Connections {
                target: root.controller
                function onLocalStorageUsageForAccountChanged(accountUserIdAtHost) {
                    if (root.accountUserIdAtHost !== accountUserIdAtHost) {
                        return;
                    }

                    generalActionsGrid.localUsedStorage = root.controller.localStorageUsageGbForAccount(root.accountUserIdAtHost);
                }

                function onRemoteStorageUsageForAccountChanged(accountUserIdAtHost) {
                    if (root.accountUserIdAtHost !== accountUserIdAtHost) {
                       return;
                    }

                    generalActionsGrid.remoteUsedStorage = root.controller.remoteStorageUsageGbForAccount(root.accountUserIdAtHost);
                }
            }

            EnforcedPlainTextLabel {
                Layout.row: 0
                Layout.column: 0
                Layout.alignment: Layout.AlignLeft | Layout.AlignVCenter
                Layout.fillWidth: true
                text: qsTr("Local storage use")
                font.bold: true
            }

            EnforcedPlainTextLabel {
                Layout.row: 0
                Layout.column: 1
                Layout.alignment: Layout.AlignRight | Layout.AlignVCenter
                text: qsTr("%1 GB of %2 GB remote files synced").arg(generalActionsGrid.localUsedStorage).arg(generalActionsGrid.remoteUsedStorage);
                color: Style.ncSecondaryTextColor
                horizontalAlignment: Text.AlignRight
            }

            CustomButton {
                Layout.row: 0
                Layout.column: 2
                Layout.alignment: Layout.AlignRight | Layout.AlignVCenter
                text: qsTr("Evict local copies...")
                onPressed: root.controller.createEvictionWindowForAccount(root.accountUserIdAtHost)
            }

            ProgressBar {
                Layout.row: 1
                Layout.columnSpan: generalActionsGrid.columns
                Layout.fillWidth: true
                value: generalActionsGrid.localUsedStorage / generalActionsGrid.remoteUsedStorage
            }
        }

        CustomButton {
            text: qsTr("Create debug archive")
            onClicked: root.controller.createDebugArchive(root.accountUserIdAtHost)
        }
    }
}

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
        id: rootColumn

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            text: qsTr("General settings")
            font.bold: true
            font.pointSize: Style.subheaderFontPtSize
            elide: Text.ElideRight
        }

        CheckBox {
            id: vfsEnabledCheckBox
            text: qsTr("Enable virtual files")
            checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
            onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)
        }

        Loader {
            id: vfsSettingsLoader

            Layout.fillWidth: true
            Layout.fillHeight: true

            active: vfsEnabledCheckBox.checked
            sourceComponent: ColumnLayout {
                Rectangle {
                    Layout.fillWidth: true
                    height: Style.normalBorderWidth
                    color: Style.ncSecondaryTextColor
                }

                FileProviderSyncStatus {
                    syncStatus: root.controller.domainSyncStatusForAccount(root.accountUserIdAtHost)
                }

                FileProviderFastEnumerationSettings {
                    id: fastEnumerationSettings

                    Layout.fillWidth: true

                    fastEnumerationSet: root.controller.fastEnumerationSetForAccount(root.accountUserIdAtHost)
                    fastEnumerationEnabled: root.controller.fastEnumerationEnabledForAccount(root.accountUserIdAtHost)
                    onFastEnumerationEnabledToggled: root.controller.setFastEnumerationEnabledForAccount(root.accountUserIdAtHost, enabled)

                    padding: 0

                    Connections {
                        target: root.controller

                        function updateFastEnumerationValues() {
                            fastEnumerationSettings.fastEnumerationEnabled = root.controller.fastEnumerationEnabledForAccount(root.accountUserIdAtHost);
                            fastEnumerationSettings.fastEnumerationSet = root.controller.fastEnumerationSetForAccount(root.accountUserIdAtHost);
                        }

                        function onFastEnumerationEnabledForAccountChanged(accountUserIdAtHost) {
                            if (root.accountUserIdAtHost === accountUserIdAtHost) {
                                updateFastEnumerationValues();
                            }
                        }

                        function onFastEnumerationSetForAccountChanged(accountUserIdAtHost) {
                            if (root.accountUserIdAtHost === accountUserIdAtHost) {
                                updateFastEnumerationValues();
                            }
                        }
                    }
                }

                FileProviderStorageInfo {
                    id: storageInfo
                    localUsedStorage: root.controller.localStorageUsageGbForAccount(root.accountUserIdAtHost)
                    remoteUsedStorage: root.controller.remoteStorageUsageGbForAccount(root.accountUserIdAtHost)

                    onEvictDialogRequested: root.controller.createEvictionWindowForAccount(root.accountUserIdAtHost)

                    Connections {
                        target: root.controller

                        function onLocalStorageUsageForAccountChanged(accountUserIdAtHost) {
                            if (root.accountUserIdAtHost !== accountUserIdAtHost) {
                                return;
                            }
                            storageInfo.localUsedStorage = root.controller.localStorageUsageGbForAccount(root.accountUserIdAtHost);
                        }

                        function onRemoteStorageUsageForAccountChanged(accountUserIdAtHost) {
                            if (root.accountUserIdAtHost !== accountUserIdAtHost) {
                                return;
                            }
                            storageInfo.remoteUsedStorage = root.controller.remoteStorageUsageGbForAccount(root.accountUserIdAtHost);
                        }
                    }
                }

                EnforcedPlainTextLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: Style.standardSpacing
                    text: qsTr("Advanced")
                    font.bold: true
                    font.pointSize: Style.subheaderFontPtSize
                    elide: Text.ElideRight
                }

                CustomButton {
                    text: qsTr("Signal file provider domain")
                    onClicked: root.controller.signalFileProviderDomain(root.accountUserIdAtHost)
                }

                CustomButton {
                    text: qsTr("Create debug archive")
                    onClicked: root.controller.createDebugArchive(root.accountUserIdAtHost)
                }
            }
        }
    }
}

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
        // Match the tab background color
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

        GroupBox {
            Layout.fillWidth: true
            title: qsTr("General settings")
            font.pointSize: Style.subheaderFontPtSize
            style: GroupBoxStyle {
                padding.left: 12
                padding.top: 12
                padding.bottom: 12
            }

            ColumnLayout {
                Layout.margins: Style.standardSpacing
                CheckBox {
                    id: vfsEnabledCheckBox
                    text: qsTr("Enable virtual files")
                    checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
                    onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)
                }

                CheckBox {
                    text: qsTr("Allow deletion of items in Trash")
                    checked: root.controller.trashDeletionEnabledForAccount(root.accountUserIdAtHost)
                    onClicked: root.controller.setTrashDeletionEnabledForAccount(root.accountUserIdAtHost, checked)
                }
            }
        }

        GroupBox {
            Layout.fillWidth: true
            title: qsTr("Synchronization")
            font.pointSize: Style.subheaderFontPtSize
            style: GroupBoxStyle {
                padding.left: 12
                padding.top: 12
                padding.bottom: 12
            }

            ColumnLayout {
                Layout.margins: Style.standardSpacing
                Loader {
                    id: vfsSettingsLoader

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    active: vfsEnabledCheckBox.checked
                    sourceComponent: ColumnLayout {
                        FileProviderSyncStatus {
                            syncStatus: root.controller.domainSyncStatusForAccount(root.accountUserIdAtHost)
                            onDomainSignalRequested: root.controller.signalFileProviderDomain(root.accountUserIdAtHost)
                        }

                        Item { height: Style.standardSpacing }

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
                    }
                }
            }
        }
    }
}

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
        color: palette.base
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

        RowLayout {
            Layout.fillWidth: true
            spacing: Style.standardSpacing

            EnforcedPlainTextLabel {
                text: qsTr("Custom folder name:")
                Layout.preferredWidth: 150
            }

            TextField {
                id: customFolderNameField
                Layout.fillWidth: true
                placeholderText: qsTr("Enter custom folder name (leave empty for default)")
                text: root.controller.customFolderNameForAccount(root.accountUserIdAtHost)
                onEditingFinished: root.controller.setCustomFolderNameForAccount(root.accountUserIdAtHost, text)
                
                Connections {
                    target: root.controller
                    
                    function onCustomFolderNameForAccountChanged(accountUserIdAtHost) {
                        if (root.accountUserIdAtHost !== accountUserIdAtHost) {
                            return;
                        }
                        customFolderNameField.text = root.controller.customFolderNameForAccount(root.accountUserIdAtHost);
                    }
                }
            }
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
                    color: palette.dark
                }

                FileProviderSyncStatus {
                    syncStatus: root.controller.domainSyncStatusForAccount(root.accountUserIdAtHost)
                    onDomainSignalRequested: root.controller.signalFileProviderDomain(root.accountUserIdAtHost)
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

                CheckBox {
                    text: qsTr("Allow deletion of items in Trash")
                    checked: root.controller.trashDeletionEnabledForAccount(root.accountUserIdAtHost)
                    onClicked: root.controller.setTrashDeletionEnabledForAccount(root.accountUserIdAtHost, checked)
                }

                Button {
                    text: qsTr("Reset virtual files environment")
                    onPressed: root.controller.resetVfsForAccount(root.accountUserIdAtHost);
                }
            }
        }
    }
}

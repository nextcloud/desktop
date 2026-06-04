/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import com.nextcloud.desktopclient
import "../../tray"

Item {
    id: root

    required property var controller
    readonly property string serverLabel: root.controller.serverDisplayName !== ""
        ? root.controller.serverDisplayName
        : root.controller.serverUrl.replace(/^https?:\/\//, "").replace(/\/$/, "")
    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color hintTextColor: Style.wizardSecondaryText

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 40
        anchors.bottomMargin: 16
        spacing: 2

        Item {
            Layout.preferredWidth: 80
            Layout.preferredHeight: 80
            Layout.alignment: Qt.AlignHCenter

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Style.wizardAvatarPlaceholder
                visible: accountAvatar.status !== Image.Ready

                EnforcedPlainTextLabel {
                    anchors.centerIn: parent
                    text: root.controller.userDisplayName !== "" ? root.controller.userDisplayName.charAt(0).toUpperCase() : ""
                    color: root.primaryTextColor
                    font.pixelSize: Style.pixelSize + 22
                    font.bold: true
                }
            }

            Image {
                id: accountAvatar
                anchors.fill: parent
                source: root.controller.avatarUrl
                sourceSize.width: 80
                sourceSize.height: 80
                fillMode: Image.PreserveAspectCrop
                cache: false
                visible: status === Image.Ready
            }
        }

        EnforcedPlainTextLabel {
            text: root.controller.userDisplayName
            color: root.primaryTextColor
            font.pixelSize: Style.pixelSize + 6
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        EnforcedPlainTextLabel {
            text: root.serverLabel
            color: root.hintTextColor
            font.pixelSize: Style.pixelSize + 2
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            elide: Text.ElideMiddle
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 24
            spacing: 8

            OptionRow {
                Layout.fillWidth: true
                visible: root.controller.canUseVirtualFiles
                title: qsTr("Virtual files")
                description: qsTr("Download files on-demand")
                selected: root.controller.syncMode === AccountWizardController.VirtualFiles
                onClicked: root.controller.setSyncMode(AccountWizardController.VirtualFiles)
            }

            OptionRow {
                Layout.fillWidth: true
                enabled: root.controller.canUseClassicSync
                title: qsTr("Synchronize everything")
                description: root.controller.syncEverythingDescription
                selected: root.controller.syncMode === AccountWizardController.SyncEverything
                onClicked: root.controller.setSyncMode(AccountWizardController.SyncEverything)
            }

            OptionRow {
                Layout.fillWidth: true
                enabled: root.controller.canUseClassicSync
                title: qsTr("Choose what to sync")
                description: ""
                selected: root.controller.syncMode === AccountWizardController.SelectiveSync
                onClicked: root.controller.openSelectiveSync()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 18
            spacing: 6
            visible: root.controller.localSyncFolderRequired

            EnforcedPlainTextLabel {
                text: qsTr("Local sync folder")
                color: root.primaryTextColor
                font.pixelSize: Style.pixelSize + 1
                font.bold: true
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 6
                    border.width: 1
                    border.color: root.controller.localSyncFolderError === ""
                        ? Style.wizardRowBorder
                        : root.controller.localSyncFolderWarning ? Style.wizardWarningBorder : Style.wizardErrorBorder
                    color: Style.wizardRowBackground

                    EnforcedPlainTextLabel {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: Text.AlignVCenter
                        text: root.controller.localSyncFolderDisplay
                        color: root.primaryTextColor
                        font.pixelSize: Style.pixelSize
                        elide: Text.ElideMiddle
                    }
                }

                WizardButton {
                    text: qsTr("Choose")
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 36
                    enabled: root.controller.canUseClassicSync
                    onClicked: root.controller.chooseLocalSyncFolder()
                }
            }

            EnforcedPlainTextLabel {
                visible: root.controller.localSyncFolderFreeSpace !== ""
                text: root.controller.localSyncFolderFreeSpace
                color: root.hintTextColor
                font.pixelSize: Style.pixelSize
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                visible: root.controller.localSyncFolderError !== ""
                text: root.controller.localSyncFolderError
                color: root.controller.localSyncFolderWarning ? Style.wizardWarningText : Style.wizardErrorText
                font.pixelSize: Style.pixelSize
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }

            ColumnLayout {
                visible: root.controller.localSyncFolderHasExistingData
                    && root.controller.canUseClassicSync
                    && root.controller.localSyncFolderRequired
                Layout.fillWidth: true
                spacing: 2

                EnforcedPlainTextLabel {
                    text: qsTr("Warning: The local folder is not empty. Pick a resolution!")
                    color: root.primaryTextColor
                    font.pixelSize: Style.pixelSize
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RadioButton {
                    text: qsTr("Keep local data")
                    checked: !root.controller.syncFromScratch
                    font.pixelSize: Style.pixelSize
                    onClicked: root.controller.setSyncFromScratch(false)
                    Layout.fillWidth: true
                }

                RadioButton {
                    text: qsTr("Erase local folder and start a clean sync")
                    checked: root.controller.syncFromScratch
                    font.pixelSize: Style.pixelSize
                    onClicked: root.controller.setSyncFromScratch(true)
                    Layout.fillWidth: true
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}

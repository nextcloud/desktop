/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

WizardStyledWindow {
    id: dialog
    visible: true

    required property var account
    property string localPath: ""
    property string shortLocalPath: dialog.localPath.split("/").reverse()[0]
    property string fileId: ""
    property string remotePath: ""
    property Share selectedShare: null
    property Share sharePendingDeletion: null
    property bool activatingShare: false
    property string shareActivationError: ""
    property alias controller: controllerObject

    property FileDetails fileDetails: FileDetails {
        localPath: dialog.localPath
    }

    title: qsTr("Share \"%1\"").arg(dialog.fileDetails.name || dialog.shortLocalPath || qsTr("File"))
    width: Style.sharingDialogWidth
    height: Style.sharingDialogHeight
    minimumWidth: Style.dialogWidth
    minimumHeight: Style.compactDialogHeight

    function currentShares() {
        return controllerObject ? Array.from(controllerObject.shares || []) : []
    }

    function reconcileSelectedShare() {
        const shares = dialog.currentShares()
        if (dialog.selectedShare && shares.indexOf(dialog.selectedShare) !== -1) {
            return
        }

        dialog.selectedShare = null
    }

    function shareTitle(share): string {
        if (!share || !share.recipients) {
            return qsTr("Share settings")
        }

        const names = []
        for (const recipient of Array.from(share.recipients)) {
            if (recipient) {
                const name = recipient.displayName || recipient.value
                if (name) {
                    names.push(name)
                }
            }
        }
        return names.length > 0 ? qsTr("Share with %1").arg(names.join(", ")) : qsTr("New share")
    }

    function copyToClipboard(value: string): void {
        clipboardHelper.text = value
        clipboardHelper.selectAll()
        clipboardHelper.copy()
        clipboardHelper.clear()
    }

    onSelectedShareChanged: {
        dialog.activatingShare = false
        dialog.shareActivationError = ""
    }

    SharingController {
        id: controllerObject
        objectName: "sharingController"
    }

    TextEdit {
        id: clipboardHelper
        visible: false
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: dialog.close()
    }

    Component.onCompleted: {
        controllerObject.account = dialog.account
        controllerObject.initialize(dialog.fileId)
    }

    Connections {
        target: controllerObject

        function onSharesChanged() {
            dialog.reconcileSelectedShare()
        }

        function onShareActivated(share) {
            if (share === dialog.selectedShare) {
                dialog.activatingShare = false
                dialog.selectedShare = null
            }
        }

        function onShareActivationFailed(share, error) {
            if (share === dialog.selectedShare) {
                dialog.activatingShare = false
                dialog.shareActivationError = error
            } else if (share && share.publicLink) {
                dialog.shareActivationError = error
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Style.standardSpacing
        spacing: 0

        ColumnLayout {
            Layout.leftMargin: Style.sharingDialogWindowMargin
            Layout.rightMargin: Style.sharingDialogWindowMargin
            Layout.bottomMargin: Style.standardSpacing
            spacing: Style.smallSpacing

            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                Layout.fillWidth: true

                text: dialog.fileDetails.name || dialog.shortLocalPath || qsTr("File")
                elide: Text.ElideRight
                font.pointSize: Style.titleFontPtSize
                font.weight: Font.DemiBold
                color: palette.text
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true

                text: {
                    const details = []
                    if (dialog.fileDetails.sizeString) {
                        details.push(dialog.fileDetails.sizeString)
                    }
                    if (dialog.fileDetails.lastChangedString) {
                        details.push(dialog.fileDetails.lastChangedString)
                    }
                    const owner = dialog.account ? (dialog.account.davDisplayName || dialog.account.davUser) : ""
                    if (owner) {
                        details.push(owner)
                    }
                    return details.join(" · ")
                }
                color: Style.wizardSecondaryText
                elide: Text.ElideRight
                font.pointSize: Style.defaultFontPtSize
                visible: text.length > 0
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.normalBorderWidth
            color: Style.wizardRowBorder
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.leftMargin: Style.sharingDialogWindowMargin
            Layout.rightMargin: Style.sharingDialogWindowMargin
            Layout.topMargin: Style.standardSpacing
            Layout.preferredHeight: Style.sharingDialogPaneHeaderHeight

            text: dialog.shareTitle(dialog.selectedShare)
            font.pointSize: Style.subheaderFontPtSize
            font.weight: Font.DemiBold
            visible: dialog.selectedShare
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.normalBorderWidth
            color: Style.wizardRowBorder
            visible: dialog.selectedShare
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: dialog.selectedShare ? 1 : 0

            ShareListPage {
                objectName: "shareListPage"
                Layout.fillWidth: true
                Layout.fillHeight: true
                account: dialog.account
                sharingController: controllerObject
                fileId: dialog.fileId
                remotePath: dialog.remotePath
                activationError: dialog.shareActivationError

                onShareSelected: share => dialog.selectedShare = share
                onCopyRequested: value => dialog.copyToClipboard(value)
                onClearActivationError: dialog.shareActivationError = ""
            }

            ShareDetailsFrame {
                id: shareDetailsFrame
                objectName: "shareDetailsFrame"

                sharingController: controllerObject
                share: dialog.selectedShare
                activatingShare: dialog.activatingShare
                activationError: dialog.shareActivationError

                onDeleteRequested: share => {
                    dialog.sharePendingDeletion = share
                    deleteShareConfirmation.open()
                }
                onCloseRequested: dialog.selectedShare = null
                onCancelRequested: controllerObject.destroyShare(dialog.selectedShare)
                onSaveRequested: {
                    dialog.shareActivationError = ""
                    dialog.activatingShare = true
                    controllerObject.activateShare(dialog.selectedShare)
                }
            }
        }
    }

    DeleteShareConfirmation {
        id: deleteShareConfirmation

        anchors.centerIn: parent
        onDeleteRequested: {
            if (dialog.sharePendingDeletion) {
                if (dialog.selectedShare === dialog.sharePendingDeletion) {
                    dialog.selectedShare = null
                }
                controllerObject.destroyShare(dialog.sharePendingDeletion)
            }
            dialog.sharePendingDeletion = null
        }

        onCancelled: dialog.sharePendingDeletion = null
    }
}

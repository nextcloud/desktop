/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "../tray"
import "../"

ColumnLayout {
    id: root

    property string localPath: ""
    property var accountState: ({})
    property FileDetails fileDetails: FileDetails {}
    property int horizontalPadding: 0
    property int iconSize: 32
    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue

    readonly property bool sharingPossible: shareModel && shareModel.canShare && shareModel.sharingEnabled
    readonly property bool userGroupSharingPossible: sharingPossible && shareModel.userGroupSharingEnabled
    readonly property bool publicLinkSharingPossible: sharingPossible && shareModel.publicLinkSharesEnabled
    readonly property bool serverAllowsResharing: shareModel && shareModel.serverAllowsResharing

    readonly property bool loading: sharingPossible && (!shareModel ||
                                                        shareModel.fetchOngoing ||
                                                        !shareModel.hasInitialShareFetchCompleted ||
                                                        waitingForSharesToChange)
    property bool waitingForSharesToChange: true // Gets changed to false when listview count changes
    property bool stopWaitingForSharesToChangeOnPasswordError: false

    readonly property ShareModel shareModel: ShareModel {
        accountState: root.accountState
        localPath: root.localPath

        onSharesChanged: root.waitingForSharesToChange = false

        onServerError: {
            if(errorBox.text === "") {
                errorBox.text = message;
            } else {
                errorBox.text += "\n\n" + message
            }

            errorBox.visible = true;
            root.waitingForSharesToChange = false;
        }

        onPasswordSetError: if(root.stopWaitingForSharesToChangeOnPasswordError) {
            root.waitingForSharesToChange = false;
            root.stopWaitingForSharesToChangeOnPasswordError = false;
        }

        onRequestPasswordForLinkShare: shareRequiresPasswordDialog.open()
        onRequestPasswordForEmailSharee: {
            shareRequiresPasswordDialog.sharee = sharee;
            shareRequiresPasswordDialog.open();
        }
    }

    property StackView rootStackView: StackView {}

    Dialog {
        id: shareRequiresPasswordDialog

        property var sharee

        function discardDialog() {
            sharee = undefined;
            root.waitingForSharesToChange = false;
            close();
        }

        anchors.centerIn: parent
        width: parent.width * 0.8

        title: qsTr("Password required for new share")
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        closePolicy: Popup.NoAutoClose

        visible: false
        onAboutToShow: dialogPasswordField.text = shareModel.generatePassword()

        onAccepted: {
            if(sharee) {
                root.shareModel.createNewUserGroupShareWithPasswordFromQml(sharee, dialogPasswordField.text);
                sharee = undefined;
            } else {
                root.shareModel.createNewLinkShareWithPassword(dialogPasswordField.text);
            }

            root.stopWaitingForSharesToChangeOnPasswordError = true;
            dialogPasswordField.text = "";
        }
        onDiscarded: discardDialog()
        onRejected: discardDialog()

        NCInputTextField {
            id: dialogPasswordField

            anchors.left: parent.left
            anchors.right: parent.right

            placeholderText: qsTr("Share password")
            onAccepted: shareRequiresPasswordDialog.accept()
        }
    }

    ErrorBox {
        id: errorBox

        Layout.fillWidth: true
        Layout.leftMargin: root.horizontalPadding
        Layout.rightMargin: root.horizontalPadding

        showCloseButton: true
        visible: false

        onCloseButtonClicked: {
            text = "";
            visible = false;
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: root.horizontalPadding
        Layout.rightMargin: root.horizontalPadding

        Image {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: shareModel.shareOwnerAvatar
        }

        ColumnLayout {
            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                visible: shareModel.displayShareOwner
                text: qsTr("Shared with you by %1").arg(shareModel.shareOwnerDisplayName)
                font.bold: true
            }
            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                visible: shareModel.sharedWithMeExpires
                text: qsTr("Expires in %1").arg(shareModel.sharedWithMeRemainingTimeString)
            }
        }

        visible: shareModel.displayShareOwner
    }

    ShareeSearchField {
        id: shareeSearchField
        Layout.fillWidth: true
        Layout.topMargin: Style.smallSpacing
        Layout.leftMargin: root.horizontalPadding
        Layout.rightMargin: root.horizontalPadding

        visible: root.userGroupSharingPossible
        enabled: visible && !root.loading && !root.shareModel.isShareDisabledEncryptedFolder && !shareeSearchField.isShareeFetchOngoing

        accountState: root.accountState
        shareItemIsFolder: root.fileDetails && root.fileDetails.isFolder
        shareeBlocklist: root.shareModel.sharees

        onShareeSelected: {
            root.waitingForSharesToChange = true;
            root.shareModel.createNewUserGroupShareFromQml(sharee)
        }
    }

    Loader {
        id: sharesViewLoader

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: root.horizontalPadding
        Layout.rightMargin: root.horizontalPadding

        active: root.sharingPossible

        sourceComponent: ScrollView {
            id: scrollView
            anchors.fill: parent

            contentWidth: availableWidth
            clip: true
            enabled: root.sharingPossible

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ListView {
                id: shareLinksListView

                enabled: !root.loading
                model: SortedShareModel {
                    sourceModel: root.shareModel
                }

                delegate: ShareDelegate {
                    id: shareDelegate

                    Connections {
                        target: root.shareModel
                        // Though we try to handle this internally by listening to onPasswordChanged,
                        // with passwords we will get the same value from the model data when a
                        // password set has failed, meaning we won't be able to easily tell when we
                        // have had a response from the server in QML. So we listen to this signal
                        // directly from the model and do the reset of the password field manually.
                        function onPasswordSetError(shareId, errorCode, errorMessage) {
                            if(shareId !== model.shareId) {
                                return;
                            }
                            shareDelegate.resetPasswordField();
                            shareDelegate.showPasswordSetError(errorMessage);
                        }

                        function onServerError() {
                            if(shareId !== model.shareId) {
                                return;
                            }
                            shareDelegate.resetMenu();
                        }
                    }

                    iconSize: root.iconSize
                    fileDetails: root.fileDetails
                    rootStackView: root.rootStackView
                    backgroundsVisible: root.backgroundsVisible
                    accentColor: root.accentColor
                    canCreateLinkShares: root.publicLinkSharingPossible
                    serverAllowsResharing: root.serverAllowsResharing

                    onCreateNewLinkShare: {
                        root.waitingForSharesToChange = true;
                        shareModel.createNewLinkShare();
                    }
                    onDeleteShare: {
                        root.waitingForSharesToChange = true;
                        shareModel.deleteShareFromQml(model.share);
                    }

                    onToggleAllowEditing: shareModel.toggleShareAllowEditingFromQml(model.share, enable)
                    onToggleAllowResharing: shareModel.toggleShareAllowResharingFromQml(model.share, enable)
                    onToggleHideDownload: shareModel.toggleHideDownloadFromQml(model.share, enable)
                    onTogglePasswordProtect: shareModel.toggleSharePasswordProtectFromQml(model.share, enable)
                    onToggleExpirationDate: shareModel.toggleShareExpirationDateFromQml(model.share, enable)
                    onToggleNoteToRecipient: shareModel.toggleShareNoteToRecipientFromQml(model.share, enable)
                    onPermissionModeChanged: shareModel.changePermissionModeFromQml(model.share, permissionMode)

                    onSetLinkShareLabel: shareModel.setLinkShareLabelFromQml(model.share, label)
                    onSetExpireDate: shareModel.setShareExpireDateFromQml(model.share, milliseconds)
                    onSetPassword: shareModel.setSharePasswordFromQml(model.share, password)
                    onSetNote: shareModel.setShareNoteFromQml(model.share, note)
                }

                Loader {
                    id: sharesFetchingLoader
                    anchors.fill: parent
                    active: root.loading
                    z: Infinity

                    sourceComponent: Rectangle {
                        color: palette.base
                        radius: Style.progressBarRadius
                        opacity: 0.5

                        NCBusyIndicator {
                            anchors.centerIn: parent
                            color: palette.dark
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: sharingNotPossibleView

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: root.horizontalPadding
        Layout.rightMargin: root.horizontalPadding

        active: !root.sharingPossible

        sourceComponent: Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            EnforcedPlainTextLabel {
                id: sharingDisabledLabel
                width: parent.width
                text: qsTr("Sharing is disabled")
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            EnforcedPlainTextLabel {
                width: parent.width
                text: qsTr("This item cannot be shared.")
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                visible: !root.shareModel.canShare
            }
            EnforcedPlainTextLabel {
                width: parent.width
                text: qsTr("Sharing is disabled.")
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                visible: !root.shareModel.sharingEnabled
            }
        }
    }
}

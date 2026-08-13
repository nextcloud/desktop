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

WizardStyledWindow {
    id: dialog
    visible: true

    required property var account
    property string localPath: ""
    property string shortLocalPath: dialog.localPath.split("/").reverse()[0]
    property string fileId: ""
    property Share selectedShare: null
    property Share sharePendingDeletion: null
    property bool selectShareAfterCreation: false
    property bool activatingShare: false
    property string shareActivationError: ""

    property FileDetails fileDetails: FileDetails {
        localPath: dialog.localPath
    }

    title: qsTr("Share \"%1\"").arg(dialog.fileDetails.name || dialog.shortLocalPath || qsTr("File"))
    width: Style.sharingDialogWidth
    height: Style.sharingDialogHeight
    minimumWidth: Style.sharingDialogMinimumWidth
    minimumHeight: Style.sharingDialogMinimumHeight

    function currentShares() {
        return sharingController ? Array.from(sharingController.shares || []) : []
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
            if (recipient && recipient.displayName) {
                names.push(recipient.displayName)
            }
        }
        return names.length > 0 ? qsTr("Share with %1").arg(names.join(", ")) : qsTr("New share")
    }

    onSelectedShareChanged: {
        dialog.activatingShare = false
        dialog.shareActivationError = ""
    }

    SharingController {
        id: sharingController
    }

    UnifiedShareListModel {
        id: shareListModel
        sharingController: sharingController
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: dialog.close()
    }

    Component.onCompleted: {
        sharingController.account = dialog.account
        sharingController.initialize(dialog.fileId)
    }

    Connections {
        target: sharingController

        function onSharesChanged() {
            if (dialog.selectShareAfterCreation) {
                const shares = dialog.currentShares()
                dialog.selectedShare = shares.length > 0 ? shares[shares.length - 1] : null
                dialog.selectShareAfterCreation = false
            } else {
                dialog.reconcileSelectedShare()
            }
        }

        function onShareCreationErrorChanged() {
            if (sharingController.shareCreationError.length > 0) {
                dialog.selectShareAfterCreation = false
            }
        }

        function onShareActivated(share) {
            if (share === dialog.selectedShare) {
                dialog.activatingShare = false
            }
        }

        function onShareActivationFailed(share, error) {
            if (share === dialog.selectedShare) {
                dialog.activatingShare = false
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
            color: Style.sharingDialogSeparatorColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Style.sharingDialogWindowMargin
            Layout.rightMargin: Style.sharingDialogWindowMargin
            Layout.topMargin: Style.standardSpacing
            Layout.preferredHeight: Style.sharingDialogPaneHeaderHeight

            EnforcedPlainTextLabel {
                Layout.fillWidth: true

                text: dialog.selectedShare ? dialog.shareTitle(dialog.selectedShare) : qsTr("Sharing")
                font.pointSize: Style.subheaderFontPtSize
                font.weight: Font.DemiBold
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: dialog.selectedShare ? 1 : 0

            Item {
                ListView {
                    id: shareListView

                    anchors.fill: parent
                    anchors.leftMargin: Style.sharingDialogWindowMargin
                    anchors.rightMargin: Style.sharingDialogWindowMargin
                    anchors.bottomMargin: Style.standardSpacing
                    clip: true
                    spacing: Style.extraSmallSpacing
                    model: shareListModel

                    header: ColumnLayout {
                        width: shareListView.width
                        spacing: Style.standardSpacing

                        ItemDelegate {
                            Layout.fillWidth: true
                            text: sharingController.creatingShare ? qsTr("Creating share…") : qsTr("Create new share")
                            enabled: !sharingController.creatingShare && dialog.fileId.length > 0
                            icon.source: "image://svgimage-custom-color/add.svg/" + palette.buttonText

                            onClicked: {
                                dialog.selectShareAfterCreation = true
                                sharingController.createShare(dialog.fileId)
                            }
                        }

                        EnforcedPlainTextLabel {
                            Layout.fillWidth: true
                            text: sharingController.shareCreationError
                            color: Style.wizardErrorText
                            wrapMode: Text.Wrap
                            visible: text.length > 0
                        }

                        EnforcedPlainTextLabel {
                            Layout.fillWidth: true
                            text: sharingController.shareDestructionError
                            color: Style.wizardErrorText
                            wrapMode: Text.Wrap
                            visible: text.length > 0
                        }

                        EnforcedPlainTextLabel {
                            Layout.fillWidth: true
                            text: qsTr("This item has not been shared yet.")
                            color: palette.placeholderText
                            wrapMode: Text.Wrap
                            visible: shareListView.count === 0
                        }
                    }

                    section.property: "section"
                    section.criteria: ViewSection.FullString
                    section.delegate: EnforcedPlainTextLabel {
                        required property string section

                        width: shareListView.width
                        topPadding: Style.standardSpacing
                        bottomPadding: Style.extraSmallSpacing
                        text: section === "external" ? qsTr("External shares") : qsTr("Internal shares")
                        font.weight: Font.DemiBold
                    }

                    delegate: ShareRow {
                        width: ListView.view.width
                        onConfigureRequested: dialog.selectedShare = share
                    }

                    ScrollBar.vertical: ScrollBar {
                        parent: shareListView
                        anchors.top: shareListView.top
                        anchors.right: shareListView.right
                        anchors.bottom: shareListView.bottom
                        policy: ScrollBar.AsNeeded
                    }
                }
            }

            ScrollView {
                id: shareDetailsScrollView

                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: shareDetailsScrollView.availableWidth

                    Loader {
                        id: shareDetailsLoader

                        Layout.fillWidth: true
                        Layout.leftMargin: Style.sharingDialogWindowMargin
                        Layout.rightMargin: Style.sharingDialogWindowMargin
                        Layout.bottomMargin: Style.standardSpacing
                        Layout.preferredHeight: active && item ? item.implicitHeight : 0
                        active: dialog.selectedShare !== null
                        visible: active

                        sourceComponent: ShareDetailsPage {
                            sharingController: sharingController
                            share: dialog.selectedShare
                        }
                    }
                }

                ScrollBar.horizontal: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }

                ScrollBar.vertical: ScrollBar {
                    parent: shareDetailsScrollView
                    anchors.top: shareDetailsScrollView.top
                    anchors.right: shareDetailsScrollView.right
                    anchors.bottom: shareDetailsScrollView.bottom
                    policy: ScrollBar.AsNeeded
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Style.smallSpacing
            visible: dialog.selectedShare !== null

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Style.normalBorderWidth
                color: Style.sharingDialogSeparatorColor
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                Layout.leftMargin: Style.sharingDialogWindowMargin
                Layout.rightMargin: Style.sharingDialogWindowMargin

                text: dialog.shareActivationError
                color: Style.wizardErrorText
                wrapMode: Text.Wrap
                visible: text.length > 0
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                Layout.leftMargin: Style.sharingDialogWindowMargin
                Layout.rightMargin: Style.sharingDialogWindowMargin

                text: qsTr("Changes to this share are applied immediately.")
                color: palette.placeholderText
                wrapMode: Text.Wrap
                visible: dialog.selectedShare && dialog.selectedShare.state === Share.Active
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Style.sharingDialogWindowMargin
                Layout.rightMargin: Style.sharingDialogWindowMargin
                Layout.bottomMargin: Style.standardSpacing

                Button {
                    text: qsTr("Delete share")
                    enabled: !sharingController.destroyingShare
                    flat: true
                    icon.source: "image://svgimage-custom-color/delete.svg/" + palette.buttonText
                    onClicked: {
                        dialog.sharePendingDeletion = dialog.selectedShare
                        deleteShareConfirmation.open()
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Close")
                    onClicked: dialog.selectedShare = null
                }

                Button {
                    text: dialog.activatingShare ? qsTr("Sending…") : qsTr("Send share")
                    enabled: !dialog.activatingShare
                    highlighted: true
                    visible: dialog.selectedShare && dialog.selectedShare.state === Share.Draft

                    onClicked: {
                        dialog.shareActivationError = ""
                        dialog.activatingShare = true
                        sharingController.activateShare(dialog.selectedShare)
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteShareConfirmation

        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete share?")

        EnforcedPlainTextLabel {
            width: parent.width
            text: qsTr("This removes the share and its access for all recipients.")
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Delete")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: deleteShareConfirmation.accept()
            }

            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: deleteShareConfirmation.reject()
            }
        }

        onAccepted: {
            if (dialog.sharePendingDeletion) {
                if (dialog.selectedShare === dialog.sharePendingDeletion) {
                    dialog.selectedShare = null
                }
                sharingController.destroyShare(dialog.sharePendingDeletion)
                dialog.sharePendingDeletion = null
            }
            close()
        }

        onRejected: dialog.sharePendingDeletion = null
    }
}

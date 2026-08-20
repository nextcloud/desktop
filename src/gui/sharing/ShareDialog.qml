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

    signal clearNewShareRecipientSearch

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
            if (recipient) {
                const name = recipient.displayName || recipient.value
                if (name) {
                    names.push(name)
                }
            }
        }
        return names.length > 0 ? qsTr("Share with %1").arg(names.join(", ")) : qsTr("New share")
    }

    function sectionTitle(section: string): string {
        if (section === "internal") {
            return qsTr("Internal shares")
        }
        if (section === "external") {
            return qsTr("External shares")
        }
        if (section === "additional") {
            return qsTr("Additional shares")
        }
        return qsTr("Pending shares")
    }

    function sectionDescription(section: string): string {
        if (section === "internal") {
            return qsTr("Share files within your organisation. Recipients who can already view the file can also use this link for easy access.")
        }
        if (section === "external") {
            return qsTr("Share files with others outside your organisation via public links and email addresses. You can also share to Nextcloud accounts on other instances using their federated cloud ID.")
        }
        if (section === "additional") {
            return qsTr("Shares from apps or other sources which are not included in internal or external shares.")
        }
        return ""
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
        id: sharingController
    }

    UnifiedShareListModel {
        id: shareListModel
        sharingController: sharingController
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
        sharingController.account = dialog.account
        sharingController.initialize(dialog.fileId)
    }

    Connections {
        target: sharingController

        function onSharesChanged() {
            dialog.reconcileSelectedShare()
        }

        function onShareCreated(share) {
            dialog.clearNewShareRecipientSearch()
            if (!share.publicLink) {
                dialog.selectedShare = share
            }
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

        function onInternalLinkResolved(url) {
            dialog.copyToClipboard(url)
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
            color: Style.sharingDialogSeparatorColor
            visible: dialog.selectedShare
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: dialog.selectedShare ? 1 : 0

            Item {
                id: shareListPane

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Style.sharingDialogWindowMargin
                        Layout.rightMargin: Style.sharingDialogWindowMargin
                        Layout.topMargin: Style.standardSpacing
                        Layout.bottomMargin: Style.standardSpacing
                        spacing: Style.standardSpacing

                        RecipientSearchField {
                            id: newShareRecipientSearch

                            Layout.fillWidth: true
                            enabled: !sharingController.creatingShare && dialog.fileId.length > 0
                            account: dialog.account
                            shareId: ""

                            onRecipientSelected: (recipientType, recipientValue, recipientInstance) => {
                                sharingController.createShareForRecipient(dialog.fileId, recipientType, recipientValue, recipientInstance)
                            }

                            Connections {
                                target: dialog

                                function onClearNewShareRecipientSearch() {
                                    newShareRecipientSearch.clear()
                                }
                            }
                        }

                        EnforcedPlainTextLabel {
                            Layout.fillWidth: true
                            text: qsTr("Creating share…")
                            color: Style.wizardSecondaryText
                            visible: sharingController.creatingShare
                        }

                        ErrorBox {
                            Layout.fillWidth: true
                            text: sharingController.shareCreationError
                            visible: text.length > 0
                        }

                        ErrorBox {
                            Layout.fillWidth: true
                            text: sharingController.shareDestructionError
                            visible: text.length > 0
                        }

                        ErrorBox {
                            Layout.fillWidth: true
                            text: sharingController.internalLinkError
                            visible: text.length > 0
                        }

                        ErrorBox {
                            Layout.fillWidth: true
                            text: dialog.shareActivationError
                            visible: !dialog.selectedShare && text.length > 0
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: shareListView

                            anchors.fill: parent
                            anchors.leftMargin: Style.sharingDialogWindowMargin
                            anchors.rightMargin: Style.sharingDialogWindowMargin
                            clip: true
                            spacing: Style.extraSmallSpacing
                            model: shareListModel
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            delegate: Loader {
                                id: rowLoader

                                required property int itemType
                                required property string section
                                required property Share share
                                required property string recipientNames
                                required property bool publicLink
                                required property string publicLinkUrl

                                width: ListView.view.width
                                height: item ? item.implicitHeight : 0
                                sourceComponent: {
                                    if (itemType === UnifiedShareListModel.SectionHeader) {
                                        return sectionHeaderComponent
                                    }
                                    if (itemType === UnifiedShareListModel.InternalLink) {
                                        return internalLinkComponent
                                    }
                                    if (itemType === UnifiedShareListModel.CreatePublicLink) {
                                        return createPublicLinkComponent
                                    }
                                    return shareComponent
                                }

                                Component {
                                    id: sectionHeaderComponent

                                    ShareSectionHeader {
                                        title: dialog.sectionTitle(rowLoader.section)
                                        description: dialog.sectionDescription(rowLoader.section)
                                    }
                                }

                                Component {
                                    id: internalLinkComponent

                                    ShareActionRow {
                                        title: qsTr("Internal link")
                                        subtitle: qsTr("For people who already have access")
                                        actionIcon: "image://svgimage-custom-color/copy.svg/" + palette.buttonText
                                        actionName: qsTr("Copy internal link")
                                        actionEnabled: !sharingController.resolvingInternalLink && dialog.remotePath.length > 0

                                        onActionRequested: sharingController.requestInternalLink(dialog.remotePath, dialog.fileId)
                                    }
                                }

                                Component {
                                    id: createPublicLinkComponent

                                    ShareActionRow {
                                        title: qsTr("Create public link")
                                        subtitle: ""
                                        actionIcon: "image://svgimage-custom-color/add.svg/" + palette.buttonText
                                        actionName: qsTr("Create public link")
                                        actionEnabled: !sharingController.creatingShare && dialog.fileId.length > 0

                                        onActionRequested: {
                                            dialog.shareActivationError = ""
                                            sharingController.createPublicLink(dialog.fileId)
                                        }
                                    }
                                }

                                Component {
                                    id: shareComponent

                                    ShareRow {
                                        share: rowLoader.share
                                        recipientNames: rowLoader.recipientNames
                                        publicLink: rowLoader.publicLink
                                        publicLinkUrl: rowLoader.publicLinkUrl

                                        onCopyRequested: dialog.copyToClipboard(publicLinkUrl)
                                        onConfigureRequested: dialog.selectedShare = share
                                    }
                                }
                            }
                        }
                    }
                }
            }

            WizardDialogFrame {
                id: shareDetailsFrame

                footerSeparatorVisible: dialog.selectedShare !== null
                footerTopPadding: Style.standardSpacing

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Style.wizardSectionSpacing

                    ScrollView {
                        id: shareDetailsScrollView

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: availableWidth
                        clip: true

                        ColumnLayout {
                            width: shareDetailsScrollView.availableWidth

                            Loader {
                                id: shareDetailsLoader

                                Layout.fillWidth: true
                                Layout.leftMargin: shareDetailsFrame.windowMargin
                                Layout.rightMargin: shareDetailsFrame.windowMargin
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

                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    }

                    ErrorBox {
                        Layout.fillWidth: true
                        Layout.leftMargin: shareDetailsFrame.windowMargin
                        Layout.rightMargin: shareDetailsFrame.windowMargin
                        text: dialog.shareActivationError
                        visible: text.length > 0
                    }

                    ErrorBox {
                        Layout.fillWidth: true
                        Layout.leftMargin: shareDetailsFrame.windowMargin
                        Layout.rightMargin: shareDetailsFrame.windowMargin
                        text: sharingController.shareDestructionError
                        visible: text.length > 0
                    }

                    EnforcedPlainTextLabel {
                        Layout.fillWidth: true
                        Layout.leftMargin: shareDetailsFrame.windowMargin
                        Layout.rightMargin: shareDetailsFrame.windowMargin
                        text: qsTr("Changes to this share are applied immediately.")
                        color: Style.wizardSecondaryText
                        wrapMode: Text.Wrap
                        visible: dialog.selectedShare && dialog.selectedShare.state === Share.Active
                    }
                }

                footer: [
                    WizardButton {
                        text: qsTr("Delete share")
                        enabled: !sharingController.destroyingShare
                        visible: dialog.selectedShare && dialog.selectedShare.state === Share.Active
                        iconSource: "image://svgimage-custom-color/delete.svg/" + palette.buttonText
                        iconBeforeText: true
                        onClicked: {
                            dialog.sharePendingDeletion = dialog.selectedShare
                            deleteShareConfirmation.open()
                        }
                    },
                    Item {
                        Layout.fillWidth: true
                    },
                    WizardButton {
                        text: qsTr("Close")
                        visible: dialog.selectedShare && dialog.selectedShare.state === Share.Active
                        onClicked: dialog.selectedShare = null
                    },
                    WizardButton {
                        text: sharingController.destroyingShare ? qsTr("Cancelling…") : qsTr("Cancel")
                        enabled: !sharingController.destroyingShare && !dialog.activatingShare
                        visible: dialog.selectedShare && dialog.selectedShare.state === Share.Draft

                        onClicked: sharingController.destroyShare(dialog.selectedShare)
                    },
                    WizardButton {
                        primary: true
                        text: dialog.activatingShare ? qsTr("Saving…") : qsTr("Save")
                        enabled: !dialog.activatingShare && !sharingController.destroyingShare && dialog.selectedShare && dialog.selectedShare.recipients.length > 0
                        visible: dialog.selectedShare && dialog.selectedShare.state === Share.Draft

                        onClicked: {
                            dialog.shareActivationError = ""
                            if (shareDetailsLoader.item) {
                                shareDetailsLoader.item.commitPendingChanges()
                            }
                            dialog.activatingShare = true
                            sharingController.activateShare(dialog.selectedShare)
                        }
                    }
                ]
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
            WizardButton {
                primary: true
                text: qsTr("Delete")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: deleteShareConfirmation.accept()
            }

            WizardButton {
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

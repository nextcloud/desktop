/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

ColumnLayout {
    id: root

    required property var account
    property SharingController sharingController: null
    required property string fileId
    required property string remotePath
    property string activationError: ""

    signal shareSelected(Share share)
    signal copyRequested(string value)
    signal deleteRequested(Share share)
    signal clearActivationError

    UnifiedShareListModel {
        id: shareListModel
        objectName: "shareListModel"
        sharingController: root.sharingController
    }

    Connections {
        target: root.sharingController
        ignoreUnknownSignals: true

        function onShareCreated(share) {
            newShareRecipientSearch.clear()
            if (!share.publicLink) {
                root.sharingController.fetchShareDetails(share)
                root.shareSelected(share)
            }
        }

        function onInternalLinkResolved(url) {
            root.copyRequested(url)
        }
    }

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
            enabled: root.sharingController && !root.sharingController.creatingShare && root.fileId.length > 0
            account: root.account
            shareId: ""

            onRecipientSelected: (recipientType, recipientValue, recipientInstance) => {
                root.sharingController.createShareForRecipient(root.fileId, recipientType, recipientValue, recipientInstance)
            }
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            text: qsTr("Creating share…")
            color: Style.wizardSecondaryText
            visible: root.sharingController && root.sharingController.creatingShare
        }

        ErrorBox {
            Layout.fillWidth: true
            text: root.sharingController ? root.sharingController.shareCreationError : ""
            visible: text.length > 0
        }

        ErrorBox {
            Layout.fillWidth: true
            text: root.sharingController ? root.sharingController.shareDestructionError : ""
            visible: text.length > 0
        }

        ErrorBox {
            Layout.fillWidth: true
            text: root.sharingController ? root.sharingController.internalLinkError : ""
            visible: text.length > 0
        }

        ErrorBox {
            Layout.fillWidth: true
            text: root.activationError
            visible: text.length > 0
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
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Loader {
                id: rowLoader

                required property int itemType
                required property var share
                required property string recipientNames
                required property bool publicLink
                required property string publicLinkUrl

                width: ListView.view.width
                height: item ? item.implicitHeight : 0
                sourceComponent: {
                    if (itemType === UnifiedShareListModel.InternalLink) {
                        return internalLinkComponent
                    }
                    if (itemType === UnifiedShareListModel.CreatePublicLink) {
                        return createPublicLinkComponent
                    }
                    return shareComponent
                }

                Component {
                    id: internalLinkComponent

                    ShareActionRow {
                        title: qsTr("Internal link")
                        subtitle: qsTr("For people who already have access")
                        actionIcon: "image://svgimage-custom-color/copy.svg/" + palette.buttonText
                        actionName: qsTr("Copy internal link")
                        actionEnabled: root.sharingController && !root.sharingController.resolvingInternalLink && root.remotePath.length > 0

                        onActionRequested: root.sharingController.requestInternalLink(root.remotePath, root.fileId)
                    }
                }

                Component {
                    id: createPublicLinkComponent

                    ShareActionRow {
                        title: qsTr("Create public link")
                        subtitle: ""
                        actionIcon: "image://svgimage-custom-color/add.svg/" + palette.buttonText
                        actionName: qsTr("Create public link")
                        actionEnabled: root.sharingController && !root.sharingController.creatingShare && root.fileId.length > 0

                        onActionRequested: {
                            root.clearActivationError()
                            root.sharingController.createPublicLink(root.fileId)
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

                        onCopyRequested: root.copyRequested(publicLinkUrl)
                        onConfigureRequested: {
                            root.sharingController.fetchShareDetails(share)
                            root.shareSelected(share)
                        }
                        onDeleteRequested: root.deleteRequested(share)
                    }
                }
            }
        }
    }
}

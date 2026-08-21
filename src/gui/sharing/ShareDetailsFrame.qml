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

WizardDialogFrame {
    id: root

    property SharingController sharingController: null
    property var account: null
    property Share share: null
    property bool activatingShare: false
    property string activationError: ""

    signal closeRequested
    signal cancelRequested
    signal deleteRequested(Share share)
    signal saveRequested

    footerVisible: !!root.share && root.share.state === Share.Draft
    footerSeparatorVisible: root.footerVisible
    footerTopPadding: Style.standardSpacing

    ColumnLayout {
        anchors.fill: parent
        spacing: Style.wizardSectionSpacing

        ScrollView {
            id: shareDetailsScrollView
            objectName: "shareDetailsScrollView"

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: shareDetailsScrollView.availableWidth

                Loader {
                    id: shareDetailsLoader

                    Layout.fillWidth: true
                    Layout.leftMargin: root.windowMargin
                    Layout.rightMargin: root.windowMargin
                    Layout.preferredHeight: active && item ? item.implicitHeight : 0
                    active: !!root.share
                    visible: active

                    sourceComponent: ShareDetailsPage {
                        objectName: "shareDetailsPage"
                        account: root.account
                        sharingController: root.sharingController
                        share: root.share
                    }
                }
            }

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.vertical.objectName: "shareDetailsScrollBar"
        }

        ErrorBox {
            Layout.fillWidth: true
            Layout.leftMargin: root.windowMargin
            Layout.rightMargin: root.windowMargin
            text: root.activationError
            visible: text.length > 0
        }

        ErrorBox {
            Layout.fillWidth: true
            Layout.leftMargin: root.windowMargin
            Layout.rightMargin: root.windowMargin
            text: root.sharingController ? root.sharingController.shareDestructionError : ""
            visible: text.length > 0
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.leftMargin: root.windowMargin
            Layout.rightMargin: root.windowMargin
            text: qsTr("Changes to this share are applied immediately.")
            color: Style.wizardSecondaryText
            wrapMode: Text.Wrap
            visible: !!root.share && root.share.state === Share.Active
        }
    }

    footer: [
        WizardButton {
            objectName: "deleteShareButton"
            text: qsTr("Delete share")
            enabled: !!root.sharingController && !root.sharingController.destroyingShare
            visible: !!root.share && root.share.state === Share.Active
            iconSource: "image://svgimage-custom-color/delete.svg/" + palette.buttonText
            iconBeforeText: true
            onClicked: root.deleteRequested(root.share)
        },
        Item {
            Layout.fillWidth: true
        },
        WizardButton {
            objectName: "closeShareButton"
            text: qsTr("Close")
            visible: !!root.share && root.share.state === Share.Active
            onClicked: root.closeRequested()
        },
        WizardButton {
            objectName: "cancelShareButton"
            text: root.sharingController && root.sharingController.destroyingShare ? qsTr("Cancelling…") : qsTr("Cancel")
            enabled: !!root.sharingController && !root.sharingController.destroyingShare && !root.activatingShare
            visible: !!root.share && root.share.state === Share.Draft
            onClicked: root.cancelRequested()
        },
        WizardButton {
            objectName: "saveShareButton"
            primary: true
            text: root.activatingShare ? qsTr("Saving…") : qsTr("Save")
            enabled: !root.activatingShare && !!root.sharingController && !root.sharingController.destroyingShare && !!root.share && !!root.share.recipients && root.share.recipients.length > 0
            visible: !!root.share && root.share.state === Share.Draft

            onClicked: {
                root.commitPendingChanges()
                root.saveRequested()
            }
        }
    ]

    function commitPendingChanges(): void {
        if (shareDetailsLoader.item && shareDetailsLoader.item.commitPendingChanges) {
            shareDetailsLoader.item.commitPendingChanges()
        }
    }
}

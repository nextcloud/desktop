/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"
import "../../wizard/qml"

ColumnLayout {
    id: root
    objectName: "assistantAttachmentDelegate"

    required property NC.AssistantController assistantController
    required property var attachmentData
    property var attachment: null
    property color primaryTextColor: Style.wizardPrimaryText
    property color secondaryTextColor: Style.wizardSecondaryText

    spacing: Style.wizardFooterSpacing

    Component.onCompleted: {
        attachment = assistantController.createAttachment(attachmentData, root)
        if (attachment && assistantController.accountConnected) {
            attachment.load()
        }
    }

    Connections {
        target: root.assistantController
        function onAccountConnectedChanged() {
            if (root.attachment) {
                if (root.assistantController.accountConnected) {
                    root.attachment.load()
                } else {
                    root.attachment.cancel()
                }
            }
        }
    }

    EnforcedPlainTextLabel {
        objectName: "assistantAttachmentName"
        text: root.attachment ? root.attachment.name : qsTr("Attachment")
        color: root.primaryTextColor
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }

    Image {
        id: preview
        objectName: "assistantAttachmentPreview"
        source: root.attachment ? root.attachment.previewSource : ""
        sourceSize: Qt.size(Style.assistantAttachmentPreviewSize, Style.assistantAttachmentPreviewSize)
        asynchronous: true
        cache: false
        fillMode: Image.PreserveAspectFit
        visible: status === Image.Ready
        Layout.fillWidth: true
        Layout.preferredHeight: Style.assistantAttachmentPreviewSize
        Accessible.role: Accessible.Graphic
        Accessible.name: root.attachment ? root.attachment.name : qsTr("Attachment preview")
    }

    BusyIndicator {
        objectName: "assistantAttachmentBusy"
        running: root.attachment && root.attachment.loading
        visible: running
        Layout.preferredWidth: Style.wizardFooterButtonHeight
        Layout.preferredHeight: Style.wizardFooterButtonHeight
    }

    EnforcedPlainTextLabel {
        objectName: "assistantAttachmentPreviewError"
        visible: !root.attachment || root.attachment.previewFailed || preview.status === Image.Error
        text: qsTr("Preview unavailable")
        color: root.secondaryTextColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    Flow {
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        spacing: Style.wizardFooterSpacing

        WizardButton {
            objectName: "assistantAttachmentOpen"
            text: qsTr("Open in Nextcloud")
            visible: root.attachment && root.attachment.openUrl.toString().length > 0
            onClicked: Qt.openUrlExternally(root.attachment.openUrl)
        }

        WizardButton {
            objectName: "assistantAttachmentDownload"
            text: qsTr("Download")
            enabled: root.attachment && root.attachment.downloadUrl.toString().length > 0
            onClicked: Qt.openUrlExternally(root.attachment.downloadUrl)
        }

        WizardButton {
            objectName: "assistantAttachmentRetry"
            text: qsTr("Retry preview")
            visible: root.attachment && (root.attachment.previewFailed || preview.status === Image.Error)
            enabled: root.assistantController.accountConnected && !root.attachment.loading
            onClicked: root.attachment.load()
        }
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"

Item {
    id: root
    objectName: "assistantMessageDelegate"

    required property NC.AssistantController assistantController
    required property string messageRole
    required property string messageText
    required property string dateText
    required property var attachments

    readonly property var displayedAttachments: root.assistantController.chatAttachments(root.messageText, root.attachments)
    readonly property bool isAssistantMessage: root.messageRole !== "user"

    width: ListView.view ? ListView.view.width : 0
    implicitHeight: messageBubble.implicitHeight

    Rectangle {
        id: messageBubble

        anchors {
            left: root.isAssistantMessage ? parent.left : undefined
            right: root.isAssistantMessage ? undefined : parent.right
            leftMargin: Style.extraSmallSpacing
            rightMargin: Style.extraSmallSpacing
        }
        width: Math.min(root.width * Style.assistantMessageMaximumWidthRatio,
            Math.max(root.displayedAttachments.length > 0 ? Style.assistantAttachmentPreviewSize : Style.assistantMessageMinimumWidth,
                messageTextItem.textItem.implicitWidth + Style.assistantMessageTextWidthPadding))
        implicitHeight: messageColumn.implicitHeight + Style.standardSpacing * 2
        radius: Style.mediumRoundedButtonRadius
        color: root.isAssistantMessage
            ? Style.wizardRowBackground
            : Style.wizardPrimaryButtonBackground

        ColumnLayout {
            id: messageColumn
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Style.standardSpacing
            }
            spacing: Style.wizardFooterSpacing

            AssistantMessageText {
                id: messageTextItem
                assistantController: root.assistantController
                text: root.messageText
                visible: root.messageText.length > 0
                textColor: root.isAssistantMessage
                    ? Style.wizardPrimaryText
                    : Style.wizardSelectedText
                linkColor: root.isAssistantMessage
                    ? Style.wizardRadioAccent
                    : Style.wizardSelectedText
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                onLinkActivated: link => {
                    const url = root.assistantController.chatLinkUrl(link)
                    if (url.toString().length > 0) {
                        Qt.openUrlExternally(url)
                    }
                }
            }

            Repeater {
                model: root.displayedAttachments
                delegate: AssistantAttachmentDelegate {
                    required property var modelData
                    assistantController: root.assistantController
                    attachmentData: modelData
                    primaryTextColor: root.isAssistantMessage ? Style.wizardPrimaryText : Style.wizardSelectedText
                    secondaryTextColor: root.isAssistantMessage ? Style.wizardSecondaryText : Style.wizardSelectedText
                    Layout.fillWidth: true
                }
            }

            EnforcedPlainTextLabel {
                id: timestampLabel
                text: root.dateText
                color: root.isAssistantMessage
                    ? Style.wizardSecondaryText
                    : Style.wizardSelectedText
                font.pixelSize: Style.pixelSize
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}

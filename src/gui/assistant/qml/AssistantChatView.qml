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

    required property NC.AssistantController assistantController

    spacing: Style.wizardSectionSpacing

    RowLayout {
        spacing: Style.wizardFooterSpacing
        Layout.fillWidth: true

        ComboBox {
            id: conversationPicker

            model: root.assistantController.chatConversations
            textRole: "title"
            valueRole: "conversationId"
            enabled: !root.assistantController.requestInProgress && count > 0
            displayText: root.assistantController.selectedChatConversationTitle.length > 0
                ? root.assistantController.selectedChatConversationTitle
                : qsTr("No conversation selected")
            Layout.fillWidth: true
            onActivated: root.assistantController.selectChatConversation(currentValue)
        }

        WizardButton {
            text: qsTr("Reload")
            enabled: !root.assistantController.requestInProgress
            onClicked: root.assistantController.loadData()
        }
    }

    ListView {
        id: conversationList

        clip: true
        spacing: Style.wizardSectionSpacing
        boundsBehavior: Flickable.StopAtBounds
        model: root.assistantController.messages
        Layout.fillWidth: true
        Layout.fillHeight: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            id: messageDelegate

            required property string messageRole
            required property string messageText
            required property string dateText

            readonly property bool isAssistantMessage: messageRole !== "user"

            width: conversationList.width
            implicitHeight: messageBubble.implicitHeight

            Rectangle {
                id: messageBubble

                anchors {
                    left: messageDelegate.isAssistantMessage ? parent.left : undefined
                    right: messageDelegate.isAssistantMessage ? undefined : parent.right
                    leftMargin: 2
                    rightMargin: 2
                }
                width: Math.min(messageDelegate.width * 0.78, Math.max(120, messageTextItem.implicitWidth + 24))
                implicitHeight: messageTextItem.implicitHeight + timestampLabel.implicitHeight + 26
                radius: 8
                color: messageDelegate.isAssistantMessage
                    ? Style.wizardRowBackground
                    : Style.wizardPrimaryButtonBackground

                TextEdit {
                    id: messageTextItem

                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 10
                    }
                    text: messageDelegate.messageText
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    color: messageDelegate.isAssistantMessage
                        ? Style.wizardPrimaryText
                        : Style.wizardSelectedText
                    selectedTextColor: Style.wizardSelectedText
                    selectionColor: Style.ncBlue
                    textFormat: Text.MarkdownText
                    readOnly: true
                    selectByMouse: true
                }

                EnforcedPlainTextLabel {
                    id: timestampLabel

                    anchors {
                        left: parent.left
                        right: parent.right
                        top: messageTextItem.bottom
                        margins: 10
                    }
                    text: messageDelegate.dateText
                    color: messageDelegate.isAssistantMessage
                        ? Style.wizardSecondaryText
                        : Style.wizardSelectedText
                    font.pixelSize: Style.pixelSize
                    elide: Text.ElideRight
                }
            }
        }

        onCountChanged: positionViewAtEnd()

        EnforcedPlainTextLabel {
            anchors.centerIn: parent
            width: Math.min(parent.width, 360)
            visible: conversationList.count === 0 && !root.assistantController.thinking
            text: qsTr("Start a conversation with Nextcloud Assistant.")
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    EnforcedPlainTextLabel {
        visible: root.assistantController.thinking
        text: qsTr("Assistant is thinking…")
        color: Style.wizardSecondaryText
        font.pixelSize: Style.wizardBodyFontPixelSize
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? implicitHeight : 0
    }

    WizardButton {
        text: qsTr("Retry response generation")
        visible: root.assistantController.showRetryResponseGeneration
        enabled: visible && !root.assistantController.requestInProgress
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.assistantController.retryResponseGeneration()
    }
}

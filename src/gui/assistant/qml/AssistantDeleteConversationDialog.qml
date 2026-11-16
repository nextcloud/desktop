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

Dialog {
    id: root
    objectName: "assistantDeleteConversationDialog"

    required property NC.AssistantController assistantController
    required property double conversationId
    required property string conversationTitle

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    onOpened: cancelButton.forceActiveFocus(Qt.PopupFocusReason)

    background: Rectangle {
        radius: Style.wizardDialogRadius
        color: Style.wizardWindowBackground
        border.width: Style.normalBorderWidth
        border.color: Style.wizardFieldBorder
    }

    contentItem: ColumnLayout {
        spacing: Style.wizardDialogSpacing
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Delete conversation?")

        EnforcedPlainTextLabel {
            text: qsTr("Delete conversation?")
            color: Style.wizardPrimaryText
            font.pixelSize: Style.wizardHeaderTitleFontPixelSize
            font.bold: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            //: %1 is the title of the conversation to delete.
            text: qsTr("This permanently deletes \"%1\" and its messages from the server, including in the web interface.").arg(root.conversationTitle)
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Style.wizardFooterSpacing
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                id: cancelButton
                objectName: "assistantDeleteConversationCancelButton"
                text: qsTr("Cancel")
                onClicked: root.reject()
            }

            WizardButton {
                objectName: "assistantDeleteConversationConfirmButton"
                text: qsTr("Delete")
                palette.buttonText: Style.wizardErrorText
                enabled: root.conversationId > 0
                    && root.assistantController.assistantEnabled
                    && root.assistantController.accountConnected
                    && !root.assistantController.requestInProgress
                onClicked: {
                    root.assistantController.deleteChatConversation(root.conversationId)
                    root.close()
                }
            }
        }
    }
}

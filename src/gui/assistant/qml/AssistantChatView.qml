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
    objectName: "assistantChatView"

    required property NC.AssistantController assistantController

    property double conversationToDelete: -1
    property string conversationTitleToDelete: ""

    signal newConversationStarted()

    spacing: Style.wizardSectionSpacing

    RowLayout {
        spacing: Style.wizardFooterSpacing
        Layout.fillWidth: true

        AssistantConversationPicker {
            id: conversationPicker
            objectName: "assistantConversationPicker"
            assistantController: root.assistantController
            leftPadding: Style.assistantConversationPickerLeftPadding
            rightPadding: Style.assistantConversationPickerRightPadding
            topPadding: 0
            bottomPadding: 0
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            Layout.preferredHeight: Style.standardPrimaryButtonHeight
            onDeleteRequested: (conversationId, title) => {
                root.conversationTitleToDelete = title
                root.conversationToDelete = conversationId
            }
        }

        WizardButton {
            id: newConversationButton
            objectName: "assistantNewConversationButton"

            readonly property string actionName: qsTr("New conversation")

            text: ""
            iconSource: "image://svgimage-custom-color/add.svg/"
                + (enabled ? palette.buttonText : Style.wizardDisabledText)
            iconBeforeText: true
            enabled: !root.assistantController.requestInProgress
            leftPadding: 0
            rightPadding: 0
            Layout.preferredWidth: Style.wizardFooterButtonHeight
            Accessible.name: actionName
            onClicked: {
                root.assistantController.startNewChat()
                root.newConversationStarted()
            }

            ToolTip {
                visible: newConversationButton.hovered
                text: newConversationButton.actionName
            }
        }

        WizardButton {
            id: reloadConversationsButton
            objectName: "assistantReloadConversationsButton"

            readonly property string actionName: qsTr("Reload conversations")

            text: ""
            iconSource: "image://svgimage-custom-color/change.svg/"
                + (enabled ? palette.buttonText : Style.wizardDisabledText)
            iconBeforeText: true
            enabled: !root.assistantController.requestInProgress
            leftPadding: 0
            rightPadding: 0
            Layout.preferredWidth: Style.wizardFooterButtonHeight
            Accessible.name: actionName
            onClicked: root.assistantController.loadData()

            ToolTip {
                visible: reloadConversationsButton.hovered
                text: reloadConversationsButton.actionName
            }
        }
    }

    Loader {
        parent: conversationPicker
        active: root.conversationToDelete > 0
        onLoaded: item.open()
        sourceComponent: AssistantDeleteConversationDialog {
            parent: Overlay.overlay
            assistantController: root.assistantController
            conversationId: root.conversationToDelete
            conversationTitle: root.conversationTitleToDelete
            width: Math.min(Style.wizardDialogMaximumWidth,
                root.width - Style.wizardWindowMargin * 2)
            padding: Style.wizardWindowMargin
            anchors.centerIn: parent
            onClosed: {
                root.conversationTitleToDelete = ""
                conversationPicker.forceActiveFocus(Qt.PopupFocusReason)
                root.conversationToDelete = -1
            }
        }
    }

    AssistantMessageList {
        objectName: "assistantMessageList"
        assistantController: root.assistantController
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 0
        Layout.preferredHeight: 0
    }

    Loader {
        active: root.assistantController.pendingActions.length > 0
        visible: active
        Layout.fillWidth: true
        sourceComponent: AssistantActionConfirmation {
            assistantController: root.assistantController
        }
    }

    EnforcedPlainTextLabel {
        objectName: "assistantThinkingLabel"
        visible: root.assistantController.thinking
        text: qsTr("Assistant is thinking…")
        color: Style.wizardSecondaryText
        font.pixelSize: Style.wizardBodyFontPixelSize
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? implicitHeight : 0
    }

    WizardButton {
        objectName: "assistantRetryResponseButton"
        text: qsTr("Retry response generation")
        visible: root.assistantController.showRetryResponseGeneration
        enabled: visible && !root.assistantController.requestInProgress
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.assistantController.retryResponseGeneration()
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../.."
import "../../tray"
import "../../wizard/qml"

WizardStyledWindow {
    id: root

    required property string accountName
    required property string accountServer
    required property string accountAvatar
    required property NC.AssistantController assistantController

    readonly property string headline: qsTr("Nextcloud Assistant")
    readonly property bool canUseAssistant: assistantController.assistantEnabled
        && assistantController.accountConnected
    readonly property bool canSend: canUseAssistant
        && !assistantController.requestInProgress
        && assistantQuestionInput.text.trim().length > 0

    title: ""
    width: Style.assistantWindowWidth
    height: Style.assistantWindowHeight
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight

    Component.onCompleted: root.assistantController.loadData()

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Connections {
        target: root.assistantController

        function onAssistantEnabledChanged() {
            if (!root.assistantController.assistantEnabled) {
                root.close()
            }
        }
    }

    WizardDialogFrame {
        id: frame

        anchors.fill: parent
        footer: [
            WizardTextField {
                id: assistantQuestionInput

                placeholderText: root.assistantController.selectedTaskTypeIsChat
                    ? qsTr("Type a message")
                    : qsTr("Describe the task")
                enabled: root.canUseAssistant && !root.assistantController.requestInProgress
                Layout.fillWidth: true
                Layout.preferredHeight: frame.footerButtonHeight
                onAccepted: root.submitQuestion()
            },

            WizardButton {
                primary: true
                text: qsTr("Send")
                enabled: root.canSend
                onClicked: root.submitQuestion()
            }
        ]

        ColumnLayout {
            spacing: Style.wizardSectionSpacing

            anchors {
                fill: parent
                leftMargin: frame.windowMargin
                rightMargin: frame.windowMargin
                topMargin: Style.wizardWindowTopMargin
                bottomMargin: Style.wizardWindowMargin
            }

            WindowAccountHeader {
                title: root.headline
                user: ({
                    "name": root.accountName,
                    "server": root.accountServer,
                    "avatar": root.accountAvatar
                })
                Layout.fillWidth: true
            }

            AssistantTaskTypeSelector {
                assistantController: root.assistantController
                canUseAssistant: root.canUseAssistant
                Layout.fillWidth: true
                Layout.preferredHeight: Style.assistantTaskTypeSelectorHeight
            }

            Loader {
                sourceComponent: root.assistantController.selectedTaskTypeIsChat
                    ? chatComponent
                    : taskComponent
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            EnforcedPlainTextLabel {
                visible: root.assistantController.response.length > 0
                text: visible ? root.assistantController.response : ""
                color: Style.wizardSecondaryText
                font.pixelSize: Style.wizardBodyFontPixelSize
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
            }

            ErrorBox {
                visible: root.assistantController.error.length > 0
                text: visible ? root.assistantController.error : ""
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
            }

            EnforcedPlainTextLabel {
                text: qsTr("AI can make mistakes. Review generated content before using it.")
                color: Style.wizardSecondaryText
                font.pixelSize: Style.pixelSize
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    Component {
        id: taskComponent

        AssistantTaskView {
            assistantController: root.assistantController
        }
    }

    Component {
        id: chatComponent

        AssistantChatView {
            assistantController: root.assistantController
            onNewConversationStarted: assistantQuestionInput.forceActiveFocus()
        }
    }

    function submitQuestion() {
        if (!root.canSend) {
            return
        }

        root.assistantController.submitQuestion(assistantQuestionInput.text.trim())
        assistantQuestionInput.clear()
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../.."
import "../../tray"
import "../../wizard/qml"

WizardStyledWindow {
    id: root

    required property NC.User currentUser
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

        ColumnLayout {
            anchors {
                fill: parent
                leftMargin: frame.windowMargin
                rightMargin: frame.windowMargin
                topMargin: Style.wizardWindowTopMargin
                bottomMargin: Style.wizardWindowMargin
            }
            spacing: Style.wizardSectionSpacing

            WindowAccountHeader {
                title: root.headline
                user: root.currentUser
                Layout.fillWidth: true
            }

            ScrollView {
                clip: true
                Layout.fillWidth: true
                Layout.preferredHeight: 42

                Row {
                    spacing: 8

                    Repeater {
                        model: root.assistantController ? root.assistantController.taskTypes : null

                        delegate: Button {
                            id: taskTypeButton

                            required property string typeId
                            required property string name
                            required property bool isChat

                            readonly property color idleBackgroundColor: {
                                if (!enabled) {
                                    return Style.wizardDisabledButtonBackground
                                }
                                if (down) {
                                    return Style.wizardSecondaryButtonPressed
                                }
                                return hovered ? Style.wizardSecondaryButtonBackground : "transparent"
                            }

                            text: name
                            checkable: true
                            checked: root.assistantController
                                && root.assistantController.selectedTaskTypeId === typeId
                            enabled: root.canUseAssistant
                                && !root.assistantController.requestInProgress
                            implicitHeight: Style.wizardFooterButtonHeight
                            leftPadding: 12
                            rightPadding: 12
                            font.pixelSize: Style.pixelSize + 2
                            font.weight: checked ? Font.DemiBold : Font.Normal

                            contentItem: Row {
                                spacing: taskTypeButton.isChat ? 5 : 0

                                Image {
                                    visible: taskTypeButton.isChat
                                    source: "image://svgimage-custom-color/comment.svg/"
                                        + (taskTypeButton.checked ? Style.wizardSelectedText : taskTypeButton.palette.buttonText)
                                    sourceSize.width: Style.smallIconSize
                                    sourceSize.height: Style.smallIconSize
                                    width: visible ? Style.smallIconSize : 0
                                    height: Style.smallIconSize
                                    anchors.verticalCenter: parent.verticalCenter
                                    fillMode: Image.PreserveAspectFit
                                }

                                Text {
                                    text: taskTypeButton.text
                                    color: taskTypeButton.checked
                                        ? Style.wizardSelectedText
                                        : taskTypeButton.enabled
                                            ? taskTypeButton.palette.buttonText
                                            : Style.wizardDisabledText
                                    font: taskTypeButton.font
                                    anchors.verticalCenter: parent.verticalCenter
                                    elide: Text.ElideRight
                                }
                            }

                            background: Rectangle {
                                radius: Style.mediumRoundedButtonRadius
                                border.width: taskTypeButton.activeFocus ? 2 : 1
                                border.color: taskTypeButton.checked || taskTypeButton.activeFocus
                                    ? Style.assistantSelectionGradientStart
                                    : taskTypeButton.hovered
                                        ? Style.wizardSecondaryButtonBorder
                                        : "transparent"

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal

                                    GradientStop {
                                        position: 0
                                        color: taskTypeButton.checked
                                            ? Style.assistantSelectionGradientStart
                                            : taskTypeButton.idleBackgroundColor
                                    }

                                    GradientStop {
                                        position: 1
                                        color: taskTypeButton.checked
                                            ? Style.assistantSelectionGradientEnd
                                            : taskTypeButton.idleBackgroundColor
                                    }
                                }
                            }

                            Accessible.name: qsTr("Select assistant task type %1").arg(name)
                            onClicked: root.assistantController.selectTaskType(typeId)
                        }
                    }
                }
            }

            Loader {
                active: root.assistantController !== null
                sourceComponent: root.assistantController && root.assistantController.selectedTaskTypeIsChat
                    ? chatComponent
                    : taskComponent
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            EnforcedPlainTextLabel {
                visible: root.assistantController !== null && root.assistantController.response.length > 0
                text: visible ? root.assistantController.response : ""
                color: Style.wizardSecondaryText
                font.pixelSize: Style.wizardBodyFontPixelSize
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
            }

            ErrorBox {
                visible: root.assistantController !== null && root.assistantController.error.length > 0
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

        footer: [
            WizardTextField {
                id: assistantQuestionInput

                placeholderText: root.assistantController && root.assistantController.selectedTaskTypeIsChat
                    ? qsTr("Type a message")
                    : qsTr("Describe the task")
                enabled: root.canUseAssistant && !root.assistantController.requestInProgress
                Layout.fillWidth: true
                Layout.preferredHeight: frame.footerButtonHeight
                onAccepted: root.submitQuestion()
            },

            WizardButton {
                text: root.assistantController && root.assistantController.selectedTaskTypeIsChat
                    ? qsTr("New chat")
                    : qsTr("Refresh")
                enabled: root.canUseAssistant && !root.assistantController.requestInProgress
                onClicked: {
                    if (root.assistantController.selectedTaskTypeIsChat) {
                        root.assistantController.startNewChat()
                    } else {
                        root.assistantController.refreshTasks()
                    }
                    assistantQuestionInput.forceActiveFocus()
                }
            },

            WizardButton {
                primary: true
                text: qsTr("Send")
                enabled: root.canSend
                onClicked: root.submitQuestion()
            }
        ]
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

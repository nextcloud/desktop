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

    required property string accountName
    required property string accountServer
    required property string accountAvatar
    required property NC.AssistantController assistantController

    readonly property string headline: qsTr("Nextcloud Assistant")
    readonly property color selectionGradientStart: "#40519a"
    readonly property color selectionGradientEnd: "#a84fc4"
    readonly property bool canUseAssistant: assistantController.assistantEnabled
        && assistantController.accountConnected
    readonly property bool canSend: canUseAssistant
        && !assistantController.requestInProgress
        && assistantQuestionInput.text.trim().length > 0

    title: ""
    width: 640
    height: 620
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

            ScrollView {
                id: taskTypeSelector

                // Temporarily hidden while task selection is not exposed in this iteration.
                // Keep this selector: it will be enabled and reused in a later iteration.
                visible: false
                clip: visible
                Layout.fillWidth: true
                Layout.preferredHeight: 42

                Row {
                    spacing: 8

                    Repeater {
                        model: taskTypeSelector.visible
                            ? root.assistantController.taskTypes
                            : null

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
                                    ? root.selectionGradientStart
                                    : taskTypeButton.hovered
                                        ? Style.wizardSecondaryButtonBorder
                                        : "transparent"

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal

                                    GradientStop {
                                        position: 0
                                        color: taskTypeButton.checked
                                            ? root.selectionGradientStart
                                            : taskTypeButton.idleBackgroundColor
                                    }

                                    GradientStop {
                                        position: 1
                                        color: taskTypeButton.checked
                                            ? root.selectionGradientEnd
                                            : taskTypeButton.idleBackgroundColor
                                    }
                                }
                            }

                            Accessible.name: qsTr("Select assistant task type %1").arg(name)
                            onClicked: root.assistantController.selectTaskType(typeId)

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                enabled: taskTypeButton.enabled
                                hoverEnabled: enabled
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }
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

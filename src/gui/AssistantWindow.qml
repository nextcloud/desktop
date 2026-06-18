/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import "./tray"
import "./wizard/qml"

ApplicationWindow {
    id: root

    property int userIndex: -1
    property var currentUser: null
    readonly property string headline: qsTr("Nextcloud Assistant")
    readonly property bool hasAssistantConversation: currentUser !== null
        && (currentUser.assistantMessages.length > 0
            || currentUser.assistantResponse.length > 0
            || currentUser.assistantError.length > 0)

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    title: ""
    width: Style.assistantWindowWidth
    height: Style.assistantWindowHeight
    minimumWidth: Style.wizardStandaloneWindowMinimumWidth
    minimumHeight: Style.wizardStandaloneWindowMinimumHeight
    flags: Qt.Window
        | Qt.CustomizeWindowHint
        | Qt.WindowTitleHint
        | Qt.WindowSystemMenuHint
        | Qt.WindowCloseButtonHint
    color: Style.wizardWindowBackground
    palette.window: Style.wizardWindowBackground
    palette.windowText: Style.wizardPrimaryText
    palette.base: Style.wizardFieldBackground
    palette.text: Style.wizardPrimaryText
    palette.button: Style.wizardFieldBackground
    palette.buttonText: Style.wizardPrimaryText
    palette.mid: Style.wizardDisabledText
    palette.placeholderText: Style.wizardPlaceholderText

    background: Rectangle {
        color: Style.wizardWindowBackground
    }

    function submitQuestion() {
        if (!currentUser) {
            return
        }

        const question = assistantQuestionInput.text.trim()
        if (question.length === 0) {
            return
        }

        currentUser.submitAssistantQuestion(question)
        assistantQuestionInput.text = ""
    }

    function resetAssistantConversation() {
        if (!currentUser) {
            return
        }

        currentUser.clearAssistantResponse()
        assistantQuestionInput.text = ""
        assistantQuestionInput.forceActiveFocus()
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Connections {
        target: root.currentUser

        function onAssistantStateChanged() {
            if (root.currentUser && !root.currentUser.isAssistantEnabled) {
                root.close()
            }
        }
    }

    Dialog {
        id: resetConfirmationDialog

        modal: true
        width: Math.min(Style.wizardDialogMaximumWidth, root.width - Style.wizardWindowMargin * 2)
        padding: Style.wizardWindowMargin
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        header: null
        footer: null

        background: Rectangle {
            radius: Style.wizardDialogRadius
            color: Style.wizardWindowBackground
            border.width: Style.normalBorderWidth
            border.color: Style.wizardFieldBorder
        }

        contentItem: ColumnLayout {
            spacing: Style.wizardDialogSpacing
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Start new conversation?")

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: qsTr("Start new conversation?")
                color: Style.wizardPrimaryText
                font.pixelSize: Style.wizardHeaderTitleFontPixelSize
                font.bold: true
                wrapMode: Text.WordWrap
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: qsTr("This will clear the existing conversation.")
                color: Style.wizardSecondaryText
                font.pixelSize: Style.wizardBodyFontPixelSize
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.wizardFooterSpacing

                Item {
                    Layout.fillWidth: true
                }

                WizardButton {
                    text: qsTr("Cancel")
                    onClicked: resetConfirmationDialog.close()
                }

                WizardButton {
                    primary: true
                    text: qsTr("New conversation")
                    onClicked: {
                        resetConfirmationDialog.close()
                        root.resetAssistantConversation()
                    }
                }
            }
        }
    }

    WizardDialogFrame {
        id: frame

        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: frame.windowMargin
            anchors.rightMargin: frame.windowMargin
            anchors.topMargin: Style.wizardWindowTopMargin
            anchors.bottomMargin: frame.windowMargin
            spacing: Style.wizardSectionSpacing

            WindowAccountHeader {
                Layout.fillWidth: true
                title: root.headline
                user: root.currentUser
            }

            ListView {
                id: assistantConversationList

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Style.wizardSectionSpacing
                boundsBehavior: Flickable.StopAtBounds
                model: root.currentUser ? root.currentUser.assistantMessages : []

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    id: messageDelegate

                    required property var modelData

                    readonly property bool isAssistantMessage: modelData.role === "assistant"

                    width: assistantConversationList.width
                    implicitHeight: messageBubble.implicitHeight

                    Rectangle {
                        id: messageBubble

                        anchors.left: messageDelegate.isAssistantMessage ? parent.left : undefined
                        anchors.right: messageDelegate.isAssistantMessage ? undefined : parent.right
                        anchors.leftMargin: 2
                        anchors.rightMargin: 2
                        radius: 10
                        color: messageDelegate.isAssistantMessage ? Style.wizardRowBackground : Style.wizardPrimaryButtonBackground
                        width: Math.min(messageDelegate.width * 0.78, Math.max(120, messageText.implicitWidth + 24))
                        implicitHeight: messageText.implicitHeight + 20

                        TextEdit {
                            id: messageText

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 10
                            text: messageDelegate.modelData.text
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            color: messageDelegate.isAssistantMessage ? Style.wizardPrimaryText : Style.wizardSelectedText
                            selectedTextColor: Style.wizardSelectedText
                            selectionColor: Style.ncBlue
                            textFormat: Text.MarkdownText
                            readOnly: true
                            selectByMouse: true
                        }
                    }
                }

                onCountChanged: positionViewAtEnd()
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                visible: root.currentUser !== null && root.currentUser.assistantResponse.length > 0
                text: visible ? root.currentUser.assistantResponse : ""
                color: Style.wizardSecondaryText
                font.pixelSize: Style.wizardBodyFontPixelSize
                wrapMode: Text.WordWrap
            }

            ErrorBox {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                visible: root.currentUser !== null && root.currentUser.assistantError.length > 0
                text: visible ? root.currentUser.assistantError : ""
            }
        }

        footer: [
            WizardTextField {
                id: assistantQuestionInput

                Layout.fillWidth: true
                Layout.preferredHeight: frame.footerButtonHeight
                placeholderText: qsTr("Ask Assistant\u00A0…")
                enabled: root.currentUser !== null
                    && root.currentUser.isAssistantEnabled
                    && root.currentUser.isConnected
                    && !root.currentUser.assistantRequestInProgress
                onAccepted: root.submitQuestion()
            },

            WizardButton {
                text: qsTr("New conversation")
                enabled: root.hasAssistantConversation
                onClicked: resetConfirmationDialog.open()
            },

            WizardButton {
                primary: true
                text: qsTr("Send")
                enabled: assistantQuestionInput.enabled && assistantQuestionInput.text.trim().length > 0
                onClicked: root.submitQuestion()
            }
        ]
    }
}

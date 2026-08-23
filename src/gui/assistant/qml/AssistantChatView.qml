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

    signal newConversationStarted()

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
            implicitHeight: Style.standardPrimaryButtonHeight
            leftPadding: 12
            rightPadding: 40
            topPadding: 0
            bottomPadding: 0
            font.pixelSize: Style.pixelSize + 3
            Layout.fillWidth: true
            Layout.preferredHeight: Style.standardPrimaryButtonHeight
            Accessible.name: qsTr("Selected conversation")
            onActivated: root.assistantController.selectChatConversation(currentValue)

            contentItem: Text {
                text: conversationPicker.displayText
                font: conversationPicker.font
                color: conversationPicker.enabled ? Style.wizardPrimaryText : Style.wizardDisabledText
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            indicator: Image {
                x: conversationPicker.width - width - 12
                y: Math.round((conversationPicker.height - height) / 2)
                width: Style.smallIconSize
                height: Style.smallIconSize
                source: "image://svgimage-custom-color/caret-down.svg/" + Style.wizardPrimaryText
                sourceSize.width: Style.smallIconSize
                sourceSize.height: Style.smallIconSize
                rotation: conversationPicker.popup.visible ? 180 : 0
                opacity: conversationPicker.enabled ? 1 : 0.45
                fillMode: Image.PreserveAspectFit
            }

            background: Rectangle {
                radius: Style.mediumRoundedButtonRadius
                color: Style.wizardFieldBackground
                border.width: Style.normalBorderWidth
                border.color: conversationPicker.activeFocus || conversationPicker.popup.visible
                    ? Style.ncBlue
                    : Style.wizardFieldBorder
            }

            delegate: ItemDelegate {
                id: conversationDelegate

                required property int index
                required property string title
                required property bool selected

                width: ListView.view ? ListView.view.width : conversationPicker.width - 8
                height: Style.standardPrimaryButtonHeight
                highlighted: conversationPicker.highlightedIndex === index

                contentItem: Text {
                    text: conversationDelegate.title
                    font: conversationPicker.font
                    color: conversationDelegate.selected
                        ? Style.wizardSelectedText
                        : Style.wizardPrimaryText
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: 6
                    color: {
                        if (conversationDelegate.selected) {
                            return Style.ncBlue
                        }
                        if (conversationDelegate.highlighted) {
                            return Style.wizardSecondaryButtonBackground
                        }
                        return Style.wizardFieldBackground
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    enabled: conversationDelegate.enabled
                    hoverEnabled: enabled
                    cursorShape: Qt.PointingHandCursor
                }
            }

            popup: Popup {
                y: conversationPicker.height + 4
                width: conversationPicker.width
                implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
                padding: 4

                contentItem: ListView {
                    clip: true
                    implicitHeight: Math.min(contentHeight, Style.standardPrimaryButtonHeight * 6)
                    model: conversationPicker.popup.visible ? conversationPicker.delegateModel : null
                    currentIndex: conversationPicker.highlightedIndex

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }
                }

                background: Rectangle {
                    radius: Style.mediumRoundedButtonRadius
                    color: Style.wizardFieldBackground
                    border.width: Style.normalBorderWidth
                    border.color: Style.wizardSecondaryButtonBorder
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                enabled: conversationPicker.enabled
                hoverEnabled: enabled
                cursorShape: Qt.PointingHandCursor
            }
        }

        WizardButton {
            id: newConversationButton

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
                popupType: Qt.platform.os === "windows" ? Popup.Item : Popup.Native
                visible: newConversationButton.hovered
                text: newConversationButton.actionName
            }
        }

        WizardButton {
            id: reloadConversationsButton

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
                popupType: Qt.platform.os === "windows" ? Popup.Item : Popup.Native
                visible: reloadConversationsButton.hovered
                text: reloadConversationsButton.actionName
            }
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

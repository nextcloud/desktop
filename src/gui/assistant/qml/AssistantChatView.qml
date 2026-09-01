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

        AssistantConversationPicker {
            assistantController: root.assistantController
            Layout.fillWidth: true
            Layout.preferredHeight: Style.standardPrimaryButtonHeight
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

        delegate: AssistantMessageDelegate {
        }

        onCountChanged: positionViewAtEnd()

        EnforcedPlainTextLabel {
            anchors.centerIn: parent
            width: Math.min(parent.width, Style.assistantEmptyStateMaximumWidth)
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

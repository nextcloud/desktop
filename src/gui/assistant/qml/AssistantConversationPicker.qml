/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import com.nextcloud.desktopclient as NC
import Style

ComboBox {
    id: root
    objectName: "assistantConversationPicker"

    required property NC.AssistantController assistantController

    model: root.assistantController.chatConversations
    textRole: "title"
    valueRole: "conversationId"
    enabled: !root.assistantController.requestInProgress && count > 0
    displayText: root.assistantController.selectedChatConversationTitle.length > 0
        ? root.assistantController.selectedChatConversationTitle
        : qsTr("No conversation selected")
    implicitHeight: Style.standardPrimaryButtonHeight
    leftPadding: Style.assistantConversationPickerLeftPadding
    rightPadding: Style.assistantConversationPickerRightPadding
    topPadding: 0
    bottomPadding: 0
    font.pixelSize: Style.assistantControlFontPixelSize
    Accessible.name: qsTr("Selected conversation")
    onActivated: root.assistantController.selectChatConversation(currentValue)

    contentItem: Text {
        text: root.displayText
        font: root.font
        color: root.enabled ? Style.wizardPrimaryText : Style.wizardDisabledText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Image {
        x: root.width - width - Style.assistantConversationPickerLeftPadding
        y: Math.round((root.height - height) / 2)
        width: Style.smallIconSize
        height: Style.smallIconSize
        source: "image://svgimage-custom-color/caret-down.svg/" + Style.wizardPrimaryText
        sourceSize.width: Style.smallIconSize
        sourceSize.height: Style.smallIconSize
        rotation: root.popup.visible ? Style.assistantExpandedIndicatorRotation : 0
        opacity: root.enabled ? 1 : Style.assistantDisabledOpacity
        fillMode: Image.PreserveAspectFit
    }

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        color: Style.wizardFieldBackground
        border.width: Style.normalBorderWidth
        border.color: root.activeFocus || root.popup.visible
            ? Style.ncBlue
            : Style.wizardFieldBorder
    }

    delegate: AssistantConversationDelegate {
        pickerFont: root.font
        pickerHighlightedIndex: root.highlightedIndex
        pickerWidth: root.width
    }

    popup: Popup {
        y: root.height + Style.assistantPopupPadding
        width: root.width
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        padding: Style.assistantPopupPadding

        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight,
                Style.standardPrimaryButtonHeight * Style.assistantConversationMaximumVisibleItems)
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex

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
        enabled: root.enabled
        hoverEnabled: enabled
        cursorShape: Qt.PointingHandCursor
    }
}

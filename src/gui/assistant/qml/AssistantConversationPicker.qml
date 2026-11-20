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

    signal deleteRequested(double conversationId, string title)

    function requestDeletion(conversationId, title) {
        // Closing the popup destroys its delegates, so continue in the picker's context.
        root.popup.close()
        root.deleteRequested(conversationId, title)
    }

    model: root.assistantController.chatConversations
    textRole: "title"
    valueRole: "conversationId"
    enabled: count > 0 && (!root.assistantController.requestInProgress || root.assistantController.thinking)
    displayText: root.assistantController.selectedChatConversationTitle.length > 0
        ? root.assistantController.selectedChatConversationTitle
        : qsTr("No conversation selected")
    implicitWidth: 0
    implicitHeight: Style.standardPrimaryButtonHeight
    font.pixelSize: Style.assistantControlFontPixelSize
    Accessible.name: qsTr("Selected conversation")
    onActivated: root.assistantController.selectChatConversation(currentValue)

    Keys.onDeletePressed: event => {
        const row = conversationList.currentItem
        if (root.popup.visible && row && row.canDelete) {
            root.requestDeletion(row.conversationId, row.title)
        } else {
            event.accepted = false
        }
    }

    contentItem: AssistantConversationTitle {
        fullTitle: root.displayText
        font: root.font
        color: root.enabled ? Style.wizardPrimaryText : Style.wizardDisabledText
    }

    indicator: Image {
        x: root.width - root.rightPadding
            + Math.round((root.rightPadding - width) / 2)
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
        canDelete: root.assistantController.assistantEnabled
            && root.assistantController.accountConnected
            && !root.assistantController.requestInProgress
        onDeleteRequested: (conversationId, title) => root.requestDeletion(conversationId, title)
    }

    popup: Popup {
        y: root.height + Style.assistantPopupPadding
        width: root.width
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        padding: Style.assistantPopupPadding

        contentItem: ListView {
            id: conversationList
            objectName: "assistantConversationDropdownList"
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
}

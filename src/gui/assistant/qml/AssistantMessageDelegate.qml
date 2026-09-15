/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick

import Style
import "../../tray"

Item {
    id: root
    objectName: "assistantMessageDelegate"

    required property string messageRole
    required property string messageText
    required property string dateText

    readonly property bool isAssistantMessage: root.messageRole !== "user"

    width: ListView.view ? ListView.view.width : 0
    implicitHeight: messageBubble.implicitHeight

    Rectangle {
        id: messageBubble

        anchors {
            left: root.isAssistantMessage ? parent.left : undefined
            right: root.isAssistantMessage ? undefined : parent.right
            leftMargin: Style.extraSmallSpacing
            rightMargin: Style.extraSmallSpacing
        }
        width: Math.min(root.width * Style.assistantMessageMaximumWidthRatio,
            Math.max(Style.assistantMessageMinimumWidth,
                messageTextItem.implicitWidth + Style.assistantMessageTextWidthPadding))
        implicitHeight: messageTextItem.implicitHeight
            + timestampLabel.implicitHeight
            + Style.assistantMessageHeightPadding
        radius: Style.mediumRoundedButtonRadius
        color: root.isAssistantMessage
            ? Style.wizardRowBackground
            : Style.wizardPrimaryButtonBackground

        TextEdit {
            id: messageTextItem

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Style.standardSpacing
            }
            text: root.messageText
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: root.isAssistantMessage
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
                margins: Style.standardSpacing
            }
            text: root.dateText
            color: root.isAssistantMessage
                ? Style.wizardSecondaryText
                : Style.wizardSelectedText
            font.pixelSize: Style.pixelSize
            elide: Text.ElideRight
        }
    }
}

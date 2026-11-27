/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style

ItemDelegate {
    id: root
    objectName: "assistantConversationDelegate"

    required property int index
    required property double conversationId
    required property string title
    required property bool selected
    required property font pickerFont
    required property int pickerHighlightedIndex
    required property real pickerWidth
    required property bool canDelete

    signal deleteRequested(double conversationId, string title)

    width: ListView.view
        ? ListView.view.width
        : root.pickerWidth - Style.assistantPopupPadding * 2
    height: Style.standardPrimaryButtonHeight
    highlighted: root.pickerHighlightedIndex === root.index
    hoverEnabled: true
    Accessible.name: root.title

    contentItem: RowLayout {
        spacing: Style.assistantPopupPadding

        AssistantConversationTitle {
            fullTitle: root.title
            font: root.pickerFont
            color: root.selected
                ? Style.wizardSelectedText
                : Style.wizardPrimaryText
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }

        ToolButton {
            id: deleteButton
            objectName: "assistantDeleteConversationButton"

            //: %1 is the title of the conversation to delete.
            readonly property string actionName: qsTr("Delete conversation \"%1\"").arg(root.title)

            opacity: root.hovered || root.activeFocus || deleteButton.hovered || deleteButton.activeFocus ? 1 : 0
            enabled: root.canDelete
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            Accessible.name: actionName
            icon.source: "qrc:/client/theme/delete.svg"
            icon.width: Style.smallIconSize
            icon.height: Style.smallIconSize
            icon.color: !deleteButton.enabled ? Style.wizardDisabledText
                : root.selected && !deleteButton.down ? Style.wizardSelectedText : Style.wizardPrimaryText
            Layout.preferredWidth: Style.standardPrimaryButtonHeight
            Layout.fillHeight: true
            onClicked: root.deleteRequested(root.conversationId, root.title)

            background: Rectangle {
                radius: Style.mediumRoundedButtonRadius
                color: deleteButton.down ? Style.wizardSecondaryButtonPressed : "transparent"
                border.width: deleteButton.activeFocus ? Style.normalBorderWidth : 0
                border.color: root.selected ? Style.wizardSelectedText : Style.ncBlue
            }

            ToolTip.visible: deleteButton.hovered || deleteButton.activeFocus
            ToolTip.text: qsTr("Delete conversation")
        }
    }

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        color: {
            if (root.selected) {
                return Style.ncBlue
            }
            if (root.highlighted) {
                return Style.wizardSecondaryButtonBackground
            }
            return Style.wizardFieldBackground
        }
    }
}

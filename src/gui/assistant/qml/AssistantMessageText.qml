/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import Style

Item {
    id: root
    objectName: "assistantMessageTextScroll"

    required property string text
    required property color textColor
    property alias textItem: messageText

    signal linkActivated(string link)

    implicitWidth: 0
    implicitHeight: messageText.implicitHeight + (horizontalBar.visible ? horizontalBar.height : 0)
    clip: true

    Flickable {
        id: viewport
        objectName: "assistantMessageTextViewport"
        anchors.fill: parent
        anchors.bottomMargin: horizontalBar.visible ? horizontalBar.height : 0
        clip: true
        acceptedButtons: Qt.NoButton
        contentWidth: Math.max(width, messageText.contentWidth)
        contentHeight: messageText.implicitHeight
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        // Only horizontal wheel events belong to the message; vertical events reach the conversation.
        ScrollBar.horizontal: ScrollBar {
            id: horizontalBar
            objectName: "assistantMessageHorizontalScrollBar"
            parent: root
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            policy: ScrollBar.AsNeeded
            active: size < 1
        }

        TextEdit {
            id: messageText
            objectName: "assistantMessageText"
            width: viewport.width
            height: implicitHeight
            text: root.text
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: root.textColor
            selectedTextColor: Style.wizardSelectedText
            selectionColor: Style.ncBlue
            textFormat: Text.MarkdownText
            readOnly: true
            selectByMouse: true
            onLinkActivated: link => root.linkActivated(link)

            HoverHandler {
                objectName: "assistantMessageLinkHover"
                blocking: false
                cursorShape: messageText.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.IBeamCursor
            }
        }
    }
}

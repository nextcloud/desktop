/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"

ScrollView {
    id: root
    objectName: "assistantMessageList"

    required property NC.AssistantController assistantController

    property alias count: messageList.count

    function itemAtIndex(index) {
        return messageList.itemAtIndex(index)
    }

    function forceLayout() {
        messageList.forceLayout()
    }

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ListView {
        id: messageList

        spacing: Style.wizardSectionSpacing
        boundsBehavior: Flickable.StopAtBounds
        model: root.assistantController.messages

        delegate: AssistantMessageDelegate {
        }

        onCountChanged: positionViewAtEnd()

        EnforcedPlainTextLabel {
            anchors.centerIn: parent
            width: Math.min(parent.width, Style.assistantEmptyStateMaximumWidth)
            visible: messageList.count === 0 && !root.assistantController.thinking
            text: qsTr("Start a conversation with Nextcloud Assistant.")
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}

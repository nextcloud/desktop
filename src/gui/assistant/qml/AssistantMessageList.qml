/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"

ListView {
    id: root
    objectName: "assistantMessageList"

    required property NC.AssistantController assistantController

    clip: true
    spacing: Style.wizardSectionSpacing
    boundsBehavior: Flickable.StopAtBounds
    model: root.assistantController.messages

    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
    }

    delegate: AssistantMessageDelegate {
    }

    onCountChanged: positionViewAtEnd()

    EnforcedPlainTextLabel {
        anchors.centerIn: parent
        width: Math.min(parent.width, Style.assistantEmptyStateMaximumWidth)
        visible: root.count === 0 && !root.assistantController.thinking
        text: qsTr("Start a conversation with Nextcloud Assistant.")
        color: Style.wizardSecondaryText
        font.pixelSize: Style.wizardBodyFontPixelSize
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}

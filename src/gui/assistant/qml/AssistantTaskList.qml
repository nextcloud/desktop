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

    required property NC.AssistantController assistantController

    signal deleteRequested(double taskId)

    clip: true
    spacing: Style.wizardSectionSpacing
    boundsBehavior: Flickable.StopAtBounds
    model: root.assistantController.tasks

    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
    }

    delegate: AssistantTaskDelegate {
        assistantController: root.assistantController
        onDeleteRequested: taskId => root.deleteRequested(taskId)
    }

    EnforcedPlainTextLabel {
        anchors.centerIn: parent
        width: Math.min(parent.width, Style.assistantEmptyStateMaximumWidth)
        visible: root.count === 0 && !root.assistantController.requestInProgress
        text: qsTr("No assistant tasks for this type.")
        color: Style.wizardSecondaryText
        font.pixelSize: Style.wizardBodyFontPixelSize
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}

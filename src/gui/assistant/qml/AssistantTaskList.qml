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
    objectName: "assistantTaskList"

    required property NC.AssistantController assistantController

    property alias count: taskList.count

    signal deleteRequested(double taskId)

    function itemAtIndex(index) {
        return taskList.itemAtIndex(index)
    }

    function forceLayout() {
        taskList.forceLayout()
    }

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ListView {
        id: taskList

        spacing: Style.wizardSectionSpacing
        boundsBehavior: Flickable.StopAtBounds
        model: root.assistantController.tasks

        delegate: AssistantTaskDelegate {
            assistantController: root.assistantController
            onDeleteRequested: taskId => root.deleteRequested(taskId)
        }

        EnforcedPlainTextLabel {
            anchors.centerIn: parent
            width: Math.min(parent.width, Style.assistantEmptyStateMaximumWidth)
            visible: taskList.count === 0 && !root.assistantController.requestInProgress
            text: qsTr("No assistant tasks for this type.")
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}

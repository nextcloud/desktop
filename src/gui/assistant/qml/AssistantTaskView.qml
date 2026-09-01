/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"

ColumnLayout {
    id: root

    required property NC.AssistantController assistantController

    spacing: Style.wizardSectionSpacing

    EnforcedPlainTextLabel {
        visible: root.assistantController.selectedTaskTypeDescription.length > 0
        text: visible ? root.assistantController.selectedTaskTypeDescription : ""
        color: Style.wizardSecondaryText
        font.pixelSize: Style.wizardBodyFontPixelSize
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? implicitHeight : 0
    }

    AssistantTaskList {
        assistantController: root.assistantController
        Layout.fillWidth: true
        Layout.fillHeight: true
        onDeleteRequested: taskId => {
            deleteTaskDialog.taskId = taskId
            deleteTaskDialog.open()
        }
    }

    AssistantDeleteTaskDialog {
        id: deleteTaskDialog

        assistantController: root.assistantController
        host: root
    }
}

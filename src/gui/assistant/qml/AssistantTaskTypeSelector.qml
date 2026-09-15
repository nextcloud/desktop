/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import com.nextcloud.desktopclient as NC
import Style

ScrollView {
    id: root
    objectName: "assistantTaskTypeSelector"

    required property NC.AssistantController assistantController
    required property bool canUseAssistant

    // Temporarily hidden while task selection is not exposed in this iteration.
    // Keep this selector: it will be enabled and reused in a later iteration.
    visible: false
    clip: visible

    Row {
        spacing: Style.wizardFooterSpacing

        Repeater {
            model: root.visible ? root.assistantController.taskTypes : null

            delegate: AssistantTaskTypeDelegate {
                assistantController: root.assistantController
                canUseAssistant: root.canUseAssistant
                leftPadding: Style.wizardSectionSpacing
                rightPadding: Style.wizardSectionSpacing
            }
        }
    }
}

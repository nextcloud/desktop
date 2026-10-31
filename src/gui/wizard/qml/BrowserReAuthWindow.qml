/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Style
import "../.."

WizardStyledWindow {
    id: root

    required property QtObject controller

    minimizable: true
    width: Style.dialogWidth
    height: Style.compactDialogHeight
    minimumWidth: Style.dialogWidth
    minimumHeight: Style.compactDialogHeight
    title: ""

    onClosing: function(close) {
        if (!root.controller.finished) {
            root.controller.cancel()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.controller.cancel()
    }

    WizardDialogFrame {
        anchors.fill: parent

        footer: [
            BrowserAuthFooter {
                controller: root.controller
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        ]

        BrowserAuthPage {
            controller: root.controller
            descriptionText: root.controller.infoText
            anchors.fill: parent
        }
    }
}

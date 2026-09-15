/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import Style

RowLayout {
    id: root

    required property QtObject controller

    spacing: Style.wizardFooterSpacing
    uniformCellSizes: true

    WizardButton {
        enabled: !root.controller.busy
        text: qsTranslate("AccountWizardWindow", "Cancel")
        Layout.fillWidth: true
        onClicked: root.controller.cancel()
    }

    WizardButton {
        enabled: !root.controller.busy && root.controller.loginUrl.toString().length > 0
        text: qsTranslate("AccountWizardWindow", "Copy link")
        iconSource: "image://svgimage-custom-color/copy.svg/" + Style.wizardPrimaryText
        iconBeforeText: true
        Layout.fillWidth: true
        onClicked: root.controller.copyLoginLink()
    }

    WizardButton {
        primary: true
        enabled: !root.controller.busy
        text: qsTranslate("AccountWizardWindow", "Open")
        textSuffix: "\u2197"
        Layout.fillWidth: true
        onClicked: root.controller.openBrowserLogin()
    }
}

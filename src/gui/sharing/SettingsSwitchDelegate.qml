/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls

import Style
import "qrc:/qml/src/gui/tray"

BasicControls.SwitchDelegate {
    id: root

    background: Rectangle {
        objectName: "settingsSwitchDelegateBackground"
        color: "transparent"
    }

    contentItem: EnforcedPlainTextLabel {
        text: root.text
        color: Style.wizardPrimaryText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}

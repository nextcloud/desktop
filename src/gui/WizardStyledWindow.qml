/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import Style

ApplicationWindow {
    id: root

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    flags: Qt.Window
        | Qt.CustomizeWindowHint
        | Qt.WindowTitleHint
        | Qt.WindowSystemMenuHint
        | Qt.WindowCloseButtonHint
    color: Style.wizardWindowBackground
    palette.window: Style.wizardWindowBackground
    palette.base: Style.wizardFieldBackground
    palette.button: Style.wizardFieldBackground
    palette.mid: Style.wizardDisabledText
    palette.placeholderText: Style.wizardPlaceholderText
    palette.active.windowText: Style.wizardPrimaryText
    palette.inactive.windowText: Style.wizardPrimaryText
    palette.disabled.windowText: Style.wizardDisabledText
    palette.active.text: Style.wizardPrimaryText
    palette.inactive.text: Style.wizardPrimaryText
    palette.disabled.text: Style.wizardDisabledText
    palette.active.buttonText: Style.wizardPrimaryText
    palette.inactive.buttonText: Style.wizardPrimaryText
    palette.disabled.buttonText: Style.wizardDisabledText

    background: Rectangle {
        color: Style.wizardWindowBackground
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls

import Style

BasicControls.Menu {
    id: root

    required property Item anchorItem

    function toggle() {
        if (opened) {
            close()
        } else {
            popup(anchorItem, 0, anchorItem.height + Style.smallSpacing)
        }
    }

    width: anchorItem.width
    padding: Style.extraSmallSpacing
    closePolicy: BasicControls.Menu.CloseOnPressOutsideParent | BasicControls.Menu.CloseOnEscape

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        color: Style.wizardFieldBackground
        border.width: Style.normalBorderWidth
        border.color: Style.wizardSecondaryButtonBorder
    }
}

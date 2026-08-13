/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls

import Style

BasicControls.ItemDelegate {
    id: root

    padding: Style.wizardSectionSpacing
    hoverEnabled: true

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        border.width: Style.normalBorderWidth
        border.color: !root.enabled ? Style.wizardRowDisabledBorder : root.highlighted ? Style.wizardSelectedBorder : Style.wizardRowBorder
        color: {
            if (!root.enabled) {
                return Style.wizardRowDisabledBackground
            }
            if (root.highlighted || root.down) {
                return Style.wizardSelectedBackground
            }
            return root.hovered ? Style.wizardSecondaryButtonBackground : Style.wizardRowBackground
        }
    }
}

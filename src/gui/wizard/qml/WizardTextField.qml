/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls
import Style

BasicControls.TextField {
    id: root

    implicitHeight: Style.standardPrimaryButtonHeight
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: Style.pixelSize + 3
    color: Style.wizardPrimaryText
    placeholderTextColor: Style.wizardPlaceholderText
    selectionColor: Style.ncBlue
    selectedTextColor: Style.wizardSelectedText
    Accessible.role: Accessible.EditableText
    Accessible.name: placeholderText

    background: Rectangle {
        radius: 8
        color: Style.wizardFieldBackground
        border.width: 1
        border.color: root.activeFocus ? Style.ncBlue : Style.wizardFieldBorder
    }
}

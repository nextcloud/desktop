/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic as BasicControls

import Style

BasicControls.TextArea {
    id: root

    implicitHeight: Style.wizardTextAreaHeight
    leftPadding: Style.wizardTextInputHorizontalPadding
    rightPadding: Style.wizardTextInputHorizontalPadding
    topPadding: Style.wizardTextInputVerticalPadding
    bottomPadding: Style.wizardTextInputVerticalPadding
    wrapMode: TextEdit.Wrap
    selectByMouse: true
    font.pixelSize: Style.wizardBodyFontPixelSize
    color: Style.wizardPrimaryText
    placeholderTextColor: Style.wizardPlaceholderText
    selectionColor: Style.ncBlue
    selectedTextColor: Style.wizardSelectedText
    Accessible.role: Accessible.EditableText
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Style.wizardTextInputRadius
        color: Style.wizardFieldBackground
        border.width: 1
        border.color: root.activeFocus ? Style.ncBlue : Style.wizardFieldBorder
    }
}

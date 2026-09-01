/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import Style

ItemDelegate {
    id: root

    required property int index
    required property string title
    required property bool selected
    required property font pickerFont
    required property int pickerHighlightedIndex
    required property real pickerWidth

    width: ListView.view
        ? ListView.view.width
        : root.pickerWidth - Style.assistantPopupPadding * 2
    height: Style.standardPrimaryButtonHeight
    highlighted: root.pickerHighlightedIndex === root.index

    contentItem: Text {
        text: root.title
        font: root.pickerFont
        color: root.selected
            ? Style.wizardSelectedText
            : Style.wizardPrimaryText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        color: {
            if (root.selected) {
                return Style.ncBlue
            }
            if (root.highlighted) {
                return Style.wizardSecondaryButtonBackground
            }
            return Style.wizardFieldBackground
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        enabled: root.enabled
        hoverEnabled: enabled
        cursorShape: Qt.PointingHandCursor
    }
}

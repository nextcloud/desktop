/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as Controls
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

    Controls.ContextMenu.menu: Controls.Menu {
        Controls.MenuItem {
            text: qsTr("Undo")
            enabled: root.canUndo && !root.readOnly
            onTriggered: root.undo()
        }

        Controls.MenuItem {
            text: qsTr("Redo")
            enabled: root.canRedo && !root.readOnly
            onTriggered: root.redo()
        }

        Controls.MenuSeparator {}

        Controls.MenuItem {
            text: qsTr("Cut")
            enabled: root.selectedText.length > 0 && root.echoMode === TextInput.Normal && !root.readOnly
            onTriggered: root.cut()
        }

        Controls.MenuItem {
            text: qsTr("Copy")
            enabled: root.selectedText.length > 0 && root.echoMode === TextInput.Normal
            onTriggered: root.copy()
        }

        Controls.MenuItem {
            text: qsTr("Paste")
            enabled: root.canPaste && !root.readOnly
            onTriggered: root.paste()
        }

        Controls.MenuItem {
            text: qsTr("Delete")
            enabled: root.selectedText.length > 0 && !root.readOnly
            onTriggered: root.remove(root.selectionStart, root.selectionEnd)
        }

        Controls.MenuSeparator {}

        Controls.MenuItem {
            text: qsTr("Select All")
            enabled: root.length > 0
            onTriggered: root.selectAll()
        }
    }

    background: Rectangle {
        radius: 8
        color: Style.wizardFieldBackground
        border.width: 1
        border.color: root.activeFocus ? Style.ncBlue : Style.wizardFieldBorder
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

TextField {
    id: root

    selectByMouse: true

    ContextMenu.menu: Menu {
        // Match AutoSizingMenu: render the menu in its own window so that its
        // items receive mouse events correctly inside frameless windows.
        popupType: Popup.Window

        MenuItem {
            text: qsTr("Undo")
            enabled: root.canUndo && !root.readOnly
            onTriggered: root.undo()
        }

        MenuItem {
            text: qsTr("Redo")
            enabled: root.canRedo && !root.readOnly
            onTriggered: root.redo()
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Cut")
            enabled: root.selectedText.length > 0 && root.echoMode === TextInput.Normal && !root.readOnly
            onTriggered: root.cut()
        }

        MenuItem {
            text: qsTr("Copy")
            enabled: root.selectedText.length > 0 && root.echoMode === TextInput.Normal
            onTriggered: root.copy()
        }

        MenuItem {
            text: qsTr("Paste")
            enabled: root.canPaste && !root.readOnly
            onTriggered: root.paste()
        }

        MenuItem {
            text: qsTr("Delete")
            enabled: root.selectedText.length > 0 && !root.readOnly
            onTriggered: root.remove(root.selectionStart, root.selectionEnd)
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Select All")
            enabled: root.length > 0
            onTriggered: root.selectAll()
        }
    }
}

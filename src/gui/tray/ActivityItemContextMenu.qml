/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import Style

AutoSizingMenu {
    id: moreActionsButtonContextMenu

    property int maxActionButtons: 0

    property var linksContextMenu: []

    signal menuEntryTriggered(int index)

    Repeater {
        id: moreActionsButtonContextMenuRepeater

        model: moreActionsButtonContextMenu.linksContextMenu

        delegate: MenuItem {
            id: moreActionsButtonContextMenuEntry
            text: model.modelData.label
            onTriggered: menuEntryTriggered(model.modelData.actionIndex)
        }
    }
}

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import Style 1.0

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

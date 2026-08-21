/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import Style
import com.nextcloud.desktopclient

TextField {
    id: root

    signal clearText()
    signal toggleFilters()
    signal moveSelection(int direction)
    signal activateSelection()

    property bool isSearchInProgress: false
    readonly property color iconColor: palette.placeholderText
    readonly property int controlSize: Math.max(40, height - 4)

    leftPadding: 8 + controlSize
    rightPadding: 8 + controlSize
    verticalAlignment: Qt.AlignVCenter
    placeholderText: qsTr("Search files, messages, events …")

    background: Rectangle {
        radius: 8
        color: Style.wizardFieldBackground
        border.width: 1
        border.color: Style.wizardFieldBorder
    }

    Keys.onPressed: event => {
        if (inputMethodComposing) return
        if (event.key === Qt.Key_Down) moveSelection(UnifiedSearchResultsListModel.Next)
        else if (event.key === Qt.Key_Up) moveSelection(UnifiedSearchResultsListModel.Previous)
        else if (event.key === Qt.Key_Home) moveSelection(UnifiedSearchResultsListModel.First)
        else if (event.key === Qt.Key_End) moveSelection(UnifiedSearchResultsListModel.Last)
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) activateSelection()
        else return
        event.accepted = true
    }

    Image {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        sourceSize.width: width
        sourceSize.height: height
        source: "image://svgimage-custom-color/search.svg/" + root.iconColor
        visible: !root.isSearchInProgress
        Accessible.ignored: true
    }

    BusyIndicator {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        visible: root.isSearchInProgress
        running: visible
        Accessible.ignored: true
    }

    ToolButton {
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        width: root.controlSize
        height: root.controlSize
        icon.source: root.text.length > 0 ? "image://svgimage-custom-color/clear.svg/" + root.iconColor
                                          : "image://svgimage-custom-color/filter.svg/" + root.iconColor
        visible: root.text.length > 0 || root.activeFocus
        Accessible.name: root.text.length > 0 ? qsTr("Clear search") : qsTr("Show search filters")
        Accessible.description: root.text.length > 0 ? qsTr("Keeps the active filters") : ""
        onClicked: root.text.length > 0 ? root.clearText() : root.toggleFilters()
    }
}

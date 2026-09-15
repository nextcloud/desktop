/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import Style
import com.nextcloud.desktopclient
import "qrc:/qml/src/gui/tray"

TextField {
    id: root

    signal clearText()
    signal moveSelection(int direction)
    signal activateSelection()

    property bool isSearchInProgress: false
    readonly property color iconColor: palette.placeholderText
    readonly property int iconSize: Style.unifiedSearchInputIconSize
    readonly property int controlSize: Math.max(Style.standardPrimaryButtonHeight,
                                                height - Style.unifiedSearchInputControlInset)

    leftPadding: Style.unifiedSearchInputHorizontalPadding + controlSize
    rightPadding: Style.unifiedSearchInputHorizontalPadding
        + (root.text.length > 0 ? controlSize : 0)
        + (root.isSearchInProgress ? Style.unifiedSearchInputHorizontalPadding + iconSize : 0)
    verticalAlignment: Qt.AlignVCenter
    placeholderText: qsTr("Search files, messages, events …")

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        color: Style.wizardFieldBackground
        border.width: Style.normalBorderWidth
        border.color: Style.wizardFieldBorder
    }

    Keys.onPressed: event => {
        if (inputMethodComposing) return
        if (event.key === Qt.Key_Down) moveSelection(UnifiedSearchResultsListModel.Next)
        else if (event.key === Qt.Key_Up) moveSelection(UnifiedSearchResultsListModel.Previous)
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) activateSelection()
        else return
        event.accepted = true
    }

    Image {
        objectName: "searchLeadingIcon"
        anchors.left: parent.left
        anchors.leftMargin: Style.unifiedSearchInputHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        sourceSize.width: width
        sourceSize.height: height
        source: "image://svgimage-custom-color/search.svg/" + root.iconColor
        Accessible.ignored: true
    }

    NCBusyIndicator {
        objectName: "searchProgressIndicator"
        anchors.right: clearSearchButton.visible ? clearSearchButton.left : parent.right
        anchors.rightMargin: clearSearchButton.visible ? 0 : Style.unifiedSearchInputHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        color: root.iconColor
        visible: root.isSearchInProgress
        running: visible
        Accessible.ignored: true
    }

    ToolButton {
        id: clearSearchButton

        objectName: "clearSearchButton"
        anchors.right: parent.right
        anchors.rightMargin: Style.extraSmallSpacing
        anchors.verticalCenter: parent.verticalCenter
        width: root.controlSize
        height: root.controlSize
        icon.source: "image://svgimage-custom-color/clear.svg/" + root.iconColor
        visible: root.text.length > 0
        Accessible.name: qsTr("Clear search")
        Accessible.description: qsTr("Keeps the active filters")
        onClicked: root.clearText()
    }
}

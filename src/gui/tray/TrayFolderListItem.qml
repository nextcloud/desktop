/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Style 1.0

MenuItem {
    id: root

    property string subline: ""
    property string iconSource: "image://svgimage-custom-color/folder-group.svg/" + palette.buttonText
    property string toolTipText: root.text

    NCToolTip {
        visible: root.hovered && root.toolTipText !== ""
        text: root.toolTipText
    }

    background: Item {
        height: parent.height
        width: parent.width
        Rectangle {
            anchors.fill: parent
            anchors.margins: Style.normalBorderWidth
            color: parent.parent.hovered || parent.parent.visualFocus ? palette.highlight : "transparent"
        }
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.trayWindowMenuEntriesMargin
        anchors.rightMargin: Style.trayWindowMenuEntriesMargin
        spacing: Style.trayHorizontalMargin

        Image {
            source: root.iconSource
            cache: true
            sourceSize.width: root.height * Style.smallIconScaleFactor
            sourceSize.height: root.height * Style.smallIconScaleFactor
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter

            Layout.preferredHeight: root.height * Style.smallIconScaleFactor
            Layout.preferredWidth: root.height * Style.smallIconScaleFactor
            Layout.alignment: Qt.AlignVCenter
        }

        ListItemLineAndSubline {
            lineText: root.text
            sublineText: root.subline

            spacing: Style.extraSmallSpacing

            Layout.fillWidth: true
        }
    }
}

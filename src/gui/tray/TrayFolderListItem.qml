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
import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style

MenuItem {
    id: root

    property string subline: ""
    property string iconSource: "image://svgimage-custom-color/account-group.svg/" + palette.buttonText
    property string backgroundIconSource: value
    property string toolTipText: root.text

    ToolTip {
        visible: root.hovered && root.toolTipText !== ""
        text: root.toolTipText
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.trayWindowMenuEntriesMargin
        anchors.rightMargin: Style.trayWindowMenuEntriesMargin
        spacing: Style.trayHorizontalMargin

        NCIconWithBackgroundImage {
            source: root.backgroundIconSource

            icon.source: root.iconSource
            icon.height: height * Style.smallIconScaleFactor
            icon.width: icon.height

            Layout.preferredHeight: root.height * Style.smallIconScaleFactor
            Layout.preferredWidth: root.height * Style.smallIconScaleFactor
            Layout.alignment: Qt.AlignVCenter
        }

        ListItemLineAndSubline {
            lineText: root.text
            sublineText: root.subline

            spacing: Style.extraSmallSpacing

            Layout.alignment: Qt.AlignVCenter

            Layout.fillWidth: true

        }
    }
}

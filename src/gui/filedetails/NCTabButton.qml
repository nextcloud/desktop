/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "../tray"

TabButton {
    id: tabButton

    property string svgCustomColorSource: ""

    padding: Style.smallSpacing
    background: Rectangle {
        radius: Style.slightlyRoundedButtonRadius
        color: tabButton.pressed ? palette.highlight : "transparent"
    }

    contentItem: ColumnLayout {
        id: tabButtonLayout

        property var elementColors: tabButton.checked || tabButton.hovered ? palette.buttonText : palette.windowText

        // We'd like to just set the height of the Image, but this causes crashing.
        // So we use a wrapping Item and use anchors to adjust the size.
        Item {
            id: iconItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            height: 20

            Image {
                id: iconItemImage
                anchors.fill: parent
                anchors.margins: tabButton.checked ? 0 : 2
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter
                fillMode: Image.PreserveAspectFit
                source: tabButton.svgCustomColorSource + "/" + tabButtonLayout.elementColors
                sourceSize.width: 32
                sourceSize.height: 32
            }
        }

        EnforcedPlainTextLabel {
            id: tabButtonLabel
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: tabButton.text
            font.bold: tabButton.checked
        }

        Rectangle {
            FontMetrics {
                id: fontMetrics
                font.family: tabButtonLabel.font.family
                font.pixelSize: tabButtonLabel.font.pixelSize
                font.bold: true
            }

            property int textWidth: fontMetrics.boundingRect(tabButtonLabel.text).width

            Layout.fillWidth: true
            implicitWidth: textWidth + Style.standardSpacing * 2
            implicitHeight: 2

            color: tabButton.checked || tabButton.hovered ?  palette.highlight : palette.base
        }
    }
}

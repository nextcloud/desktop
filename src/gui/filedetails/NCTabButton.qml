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

import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import com.nextcloud.desktopclient 1.0
import Style 1.0

TabButton {
    id: tabButton

    property string svgCustomColorSource: ""

    padding: 0
    background: Rectangle {
        color: tabButton.pressed ? Style.lightHover : Style.backgroundColor
    }

    contentItem: ColumnLayout {
        id: tabButtonLayout

        property var elementColors: tabButton.checked ? Style.ncTextColor : Style.ncSecondaryTextColor

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

        Label {
            id: tabButtonLabel
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: tabButtonLayout.elementColors
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

            implicitWidth: textWidth + Style.standardSpacing * 2
            implicitHeight: 2
            color: tabButton.checked ? Style.ncBlue : "transparent"
        }
    }
}

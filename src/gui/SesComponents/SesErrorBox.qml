/*
 * Copyright (C) 2021 by Felix Weilbach <felix.weilbach@nextcloud.com>
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
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import Style 1.0
import "../tray/"

Item {
    id: errorBox

    property string text: ""

    implicitHeight: errorMessageLayout.implicitHeight + (2 * Style.standardSpacing)

    Rectangle {
        anchors.fill: parent
        border.color: Style.sesErrorBoxBorder
        border.width: Style.thickBorderWidth
        radius: Style.sesCornerRadius
    }

    GridLayout {
        id: errorMessageLayout

        anchors.fill: parent
        anchors.margins: Style.standardSpacing
        anchors.leftMargin: Style.standardSpacing + solidStripe.width

        columns: 2

        Image {
            source: Style.sesErrorIcon
            width: 24
            height: 24
            Layout.rightMargin: Style.standardSpacing
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            font.pixelSize: Style.sesFontPixelSize
            font.weight: Style.sesFontBoldWeight

            text: qsTr("Error")
            color: Style.sesErrorBoxText
        }

        EnforcedPlainTextLabel {
            id: errorMessage

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.columnSpan: 2

            wrapMode: Text.WordWrap
            text: errorBox.text

            font.pixelSize: Style.sesFontErrortextPixelSize
            font.weight: Style.sesFontNormalWeight
        }
    }
}

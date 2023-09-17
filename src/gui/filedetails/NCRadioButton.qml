/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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
import QtQuick.Controls 2.15
import Style 1.0

RadioButton {
    id: root

    property int indicatorItemWidth: Style.radioButtonIndicatorSize
    property int indicatorItemHeight: Style.radioButtonIndicatorSize
    property int radius: Style.radioButtonCustomRadius

    indicator: Rectangle {
        implicitWidth: root.indicatorItemWidth
        implicitHeight: root.indicatorItemHeight
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Style.radioButtonCustomMarginLeftOuter
        radius: root.radius
        color: palette.base
        border.color: palette.dark
        border.width: Style.normalBorderWidth

        Rectangle {
            anchors.fill: parent
            visible: root.checked
            color: palette.buttonText
            radius: root.radius
            anchors.margins: Style.radioButtonCustomMarginLeftInner
        }
    }
}

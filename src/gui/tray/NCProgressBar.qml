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

import QtQuick
import QtQuick.Controls
import Style

// TODO: the current style does not support customization of this control
ProgressBar {
    id: control

    property color fillColor: Style.ncBlue

    background: Rectangle {
        implicitWidth: Style.progressBarWidth
        implicitHeight: Style.progressBarBackgroundHeight
        radius: Style.progressBarRadius
        color: palette.base
        border.color: palette.dark
        border.width: Style.progressBarBackgroundBorderWidth
    }

    contentItem: Item {
        implicitWidth: Style.progressBarWidth
        implicitHeight: Style.progressBarContentHeight

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: Style.progressBarRadius
            color: control.fillColor
            border.color: palette.dark
            border.width: Style.progressBarContentBorderWidth
        }
    }
}

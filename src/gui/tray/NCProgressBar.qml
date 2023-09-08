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

import QtQuick 2.15
import QtQuick.Controls 2.15
import Style 1.0

ProgressBar {
    id: control

    background: Rectangle {
        implicitWidth: Style.progressBarWidth
        implicitHeight: Style.progressBarBackgroundHeight
        radius: Style.progressBarRadius
        color: Style.progressBarBackgroundColor
        border.color: Style.progressBarBackgroundBorderColor
        border.width: Style.progressBarBackgroundBorderWidth
    }

    contentItem: Item {
        implicitWidth: Style.progressBarWidth
        implicitHeight: Style.progressBarContentHeight

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: Style.progressBarRadius
            color: Style.progressBarContentColor
            border.color: Style.progressBarContentBorderColor
            border.width: Style.progressBarContentBorderWidth
        }
    }
}

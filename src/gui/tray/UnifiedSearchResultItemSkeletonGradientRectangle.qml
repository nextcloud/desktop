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

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects

import Style 1.0

Rectangle {
    id: root

    property color progressGradientColor: Style.darkMode ? Qt.lighter(palette.light, 1.2) : Qt.darker(palette.light, 1.1)
    property int animationStartX: -width
    property int animationEndX: width

    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop {
            position: 0
            color: "transparent"
        }
        GradientStop {
            position: 0.4
            color: root.progressGradientColor
        }
        GradientStop {
            position: 0.6
            color: root.progressGradientColor
        }
        GradientStop {
            position: 1.0
            color: "transparent"
        }
    }

    NumberAnimation on x {
        from: root.animationStartX
        to: root.animationEndX
        duration: 1000
        loops: Animation.Infinite
        running: true
    }
}

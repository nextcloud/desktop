/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Style

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

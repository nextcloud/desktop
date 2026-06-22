/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as BasicControls
import Style

BasicControls.ProgressBar {
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

/*
 * Copyright (C) 2020 by Nicolas Fella <nicolas.fella@gmx.de>
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
import QtQml.Models
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

// Custom qml modules are in /theme (and included by resources.qrc)
import Style
import com.nextcloud.desktopclient

Button {
    id: root

    display: AbstractButton.IconOnly
    flat: true
    hoverEnabled: Style.hoverEffectsEnabled

    icon.width: Style.headerButtonIconSize
    icon.height: Style.headerButtonIconSize

    Layout.alignment: Qt.AlignRight
    Layout.preferredWidth:  Style.trayWindowHeaderHeight
    Layout.preferredHeight: Style.trayWindowHeaderHeight
}

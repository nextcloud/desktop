/*
 * Copyright (C) 2024 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style

ColumnLayout {
    id: root

    spacing: Style.standardSpacing

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    Image {
        source: `image://svgimage-custom-color/magnifying-glass.svg/${palette.windowText}`
        sourceSize.width: Style.trayWindowHeaderHeight / 2
        sourceSize.height: Style.trayWindowHeaderHeight / 2
        Layout.alignment: Qt.AlignHCenter
    }

    EnforcedPlainTextLabel {
        text: qsTr("Start typing to search")
        font.pixelSize: Style.unifiedSearchPlaceholderViewSublineFontPixelSize
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}

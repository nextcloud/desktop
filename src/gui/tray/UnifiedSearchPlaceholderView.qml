/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

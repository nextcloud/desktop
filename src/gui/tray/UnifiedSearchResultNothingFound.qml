/*
 * Copyright (C) 2021 by Oleksandr Zolotov <alex@nextcloud.com>
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
    id: unifiedSearchResultNothingFoundContainer

    required property string text

    spacing: Style.standardSpacing
    anchors.leftMargin: Style.unifiedSearchResultNothingFoundHorizontalMargin
    anchors.rightMargin: Style.unifiedSearchResultNothingFoundHorizontalMargin

    Image {
        id: unifiedSearchResultsNoResultsLabelIcon
        source: `image://svgimage-custom-color/magnifying-glass.svg/${palette.windowText}`
        sourceSize.width: Style.trayWindowHeaderHeight / 2
        sourceSize.height: Style.trayWindowHeaderHeight / 2
        Layout.alignment: Qt.AlignHCenter
    }

    EnforcedPlainTextLabel {
        id: unifiedSearchResultsNoResultsLabel
        text: qsTr("No results for")
        font.pixelSize: Style.unifiedSearchPlaceholderViewSublineFontPixelSize
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
        horizontalAlignment: Text.AlignHCenter
    }

    EnforcedPlainTextLabel {
        id: unifiedSearchResultsNoResultsLabelDetails
        text: unifiedSearchResultNothingFoundContainer.text
        font.pixelSize: Style.unifiedSearchPlaceholderViewTitleFontPixelSize
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
        horizontalAlignment: Text.AlignHCenter
    }
}

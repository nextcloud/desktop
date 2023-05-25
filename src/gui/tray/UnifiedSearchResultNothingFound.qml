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

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Style 1.0

ColumnLayout {
    id: unifiedSearchResultNothingFoundContainer

    required property string text

    spacing: Style.standardSpacing
    anchors.leftMargin: Style.unifiedSearchResultNothingFoundHorizontalMargin
    anchors.rightMargin: Style.unifiedSearchResultNothingFoundHorizontalMargin

    Image {
        id: unifiedSearchResultsNoResultsLabelIcon
        source: "qrc:///client/theme/magnifying-glass.svg"
        sourceSize.width: Style.trayWindowHeaderHeight / 2
        sourceSize.height: Style.trayWindowHeaderHeight / 2
        Layout.alignment: Qt.AlignHCenter
    }

    EnforcedPlainTextLabel {
        id: unifiedSearchResultsNoResultsLabel
        text: qsTr("No results for")
        color: palette.dark
        font.pixelSize: Style.subLinePixelSize * 1.25
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
        horizontalAlignment: Text.AlignHCenter
    }

    EnforcedPlainTextLabel {
        id: unifiedSearchResultsNoResultsLabelDetails
        text: unifiedSearchResultNothingFoundContainer.text
        font.pixelSize: Style.topLinePixelSize * 1.25
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
        Layout.fillWidth: true
        Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
        horizontalAlignment: Text.AlignHCenter
    }
}

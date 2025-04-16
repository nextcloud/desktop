/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import "qrc:/qml/src/gui/tray"

Item {
    id: unifiedSearchResultNothingFoundContainer

    objectName: "nothingFoundView"

    required property string text

    ColumnLayout {
        id: content

        objectName: "nothingFoundContent"
        anchors.centerIn: parent
        width: parent.width - 2 * Style.unifiedSearchResultNothingFoundHorizontalMargin
        spacing: Style.standardSpacing

        Image {
            id: unifiedSearchResultsNoResultsLabelIcon

            objectName: "nothingFoundIcon"
            source: `image://svgimage-custom-color/magnifying-glass.svg/${palette.windowText}`
            sourceSize.width: Style.trayWindowHeaderHeight / 2
            sourceSize.height: Style.trayWindowHeaderHeight / 2
            Layout.alignment: Qt.AlignHCenter
        }

        EnforcedPlainTextLabel {
            id: unifiedSearchResultsNoResultsLabel

            objectName: "nothingFoundMessage"
            text: qsTr("No results for")
            font.pixelSize: Style.unifiedSearchPlaceholderViewSublineFontPixelSize
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.preferredHeight: Style.trayWindowHeaderHeight / 2
            horizontalAlignment: Text.AlignHCenter
        }

        EnforcedPlainTextLabel {
            id: unifiedSearchResultsNoResultsLabelDetails

            objectName: "nothingFoundQuery"
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
}

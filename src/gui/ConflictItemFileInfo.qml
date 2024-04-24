/*
 * Copyright (C) 2023 by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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
import QtQuick.Controls 2.15
import Style 1.0
import "./tray"

Item {
    property alias itemSelected: selectItem.checked
    property alias itemPreviewUrl: itemPreview.source
    property alias itemVersionLabel: versionLabel.text
    property alias itemDateLabel: dateLabel.text
    property alias itemFileSizeLabel: fileSizeLabel.text

    signal selectedChanged()

    CheckBox {
        id: selectItem

        palette {
            text: Style.ncTextColor
            windowText: Style.ncTextColor
            buttonText: Style.ncTextColor
            brightText: Style.ncTextBrightColor
            highlight: Style.lightHover
            highlightedText: Style.ncTextColor
            light: Style.lightHover
            midlight: Style.ncSecondaryTextColor
            mid: Style.darkerHover
            dark: Style.menuBorder
            button: Style.buttonBackgroundColor
            window: palette.dark // NOTE: Fusion theme uses darker window colour for the border of the checkbox
            base: Style.backgroundColor
            toolTipBase: Style.backgroundColor
            toolTipText: Style.ncTextColor
        }

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter

        leftPadding: 0
        spacing: 0

        onToggled: function() {
            selectedChanged()
        }
    }

    Image {
        id: itemPreview

        anchors.left: selectItem.right
        anchors.verticalCenter: parent.verticalCenter

        width: 48
        height: 48
        sourceSize.width: 48
        sourceSize.height: 48
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: itemPreview.right
        anchors.right: parent.right
        anchors.leftMargin: 10

        spacing: 0

        Item {
            Layout.fillHeight: true
        }

        EnforcedPlainTextLabel {
            id: versionLabel

            Layout.fillWidth: true

            font.pixelSize: Style.fontPixelSizeResolveConflictsDialog
        }

        EnforcedPlainTextLabel {
            id: dateLabel

            Layout.fillWidth: true

            font.pixelSize: Style.fontPixelSizeResolveConflictsDialog
        }

        EnforcedPlainTextLabel {
            id: fileSizeLabel

            Layout.fillWidth: true

            font.pixelSize: Style.fontPixelSizeResolveConflictsDialog
        }

        Item {
            Layout.fillHeight: true
        }
    }
}

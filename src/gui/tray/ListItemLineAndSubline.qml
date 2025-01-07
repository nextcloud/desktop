/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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

    property string lineText: ""
    property string sublineText: ""

    property int titleFontSize: Style.unifiedSearchResultTitleFontSize
    property int sublineFontSize: Style.unifiedSearchResultSublineFontSize

    property color titleColor: palette.windowText
    property color sublineColor: palette.dark

    EnforcedPlainTextLabel {
        id: title
        Layout.fillWidth: true
        text: root.lineText
        elide: Text.ElideRight
        font.pixelSize: root.titleFontSize
    }
    EnforcedPlainTextLabel {
        id: subline
        Layout.fillWidth: true
        text: root.sublineText
        visible: text !== ""
        elide: Text.ElideRight
        font.pixelSize: root.sublineFontSize
    }
}

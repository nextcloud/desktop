/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
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

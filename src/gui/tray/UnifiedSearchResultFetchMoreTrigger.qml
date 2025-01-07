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
    id: unifiedSearchResultItemFetchMore

    property bool isFetchMoreInProgress: false
    property bool isWithinViewPort: false

    property int fontSize: Style.unifiedSearchResultTitleFontSize

    property string textColor: palette.dark

    Accessible.role: Accessible.ListItem
    Accessible.name: unifiedSearchResultItemFetchMoreText.text
    Accessible.onPressAction: unifiedSearchResultMouseArea.clicked()

    EnforcedPlainTextLabel {
        id: unifiedSearchResultItemFetchMoreText

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Style.trayHorizontalMargin
        Layout.rightMargin: Style.trayHorizontalMargin

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: qsTr("Load more results")
        wrapMode: Text.Wrap
        font.pixelSize: unifiedSearchResultItemFetchMore.fontSize
        visible: !unifiedSearchResultItemFetchMore.isFetchMoreInProgress
    }

    BusyIndicator {
        id: unifiedSearchResultItemFetchMoreIconInProgress

        Layout.preferredWidth: parent.height * 0.70
        Layout.preferredHeight: parent.height * 0.70
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter

        running: visible
        visible: unifiedSearchResultItemFetchMore.isFetchMoreInProgress && unifiedSearchResultItemFetchMore.isWithinViewPort
    }
}

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

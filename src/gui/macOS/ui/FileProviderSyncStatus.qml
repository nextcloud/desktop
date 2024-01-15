/*
 * Copyright (C) 2024 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0
import "../../filedetails"
import "../../tray"

import com.nextcloud.desktopclient 1.0

GridLayout {
    id: root

    required property var syncStatus

    rows: syncStatus.syncing ? 2 : 1

    NCBusyIndicator {
        id: syncIcon

        property int size: Style.trayListItemIconSize * 0.8

        Layout.row: 0
        Layout.rowSpan: root.syncStatus.syncing ? 2 : 1
        Layout.column: 0
        Layout.preferredWidth: size
        Layout.preferredHeight: size
        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

        padding: 0
        spacing: 0
        imageSource: root.syncStatus.icon
        running: root.syncStatus.syncing
    }

    EnforcedPlainTextLabel {
        Layout.row: 0
        Layout.column: 1
        Layout.columnSpan: root.syncStatus.syncing ? 2 : 1
        Layout.fillWidth: true
        font.bold: true
        font.pointSize: Style.headerFontPtSize
        text: root.syncStatus.syncing ? qsTr("Syncing") : qsTr("All synced!")
    }

    NCProgressBar {
        Layout.row: 1
        Layout.column: 1
        Layout.fillWidth: true
        value: root.syncStatus.fractionCompleted
        visible: root.syncStatus.syncing
    }
}
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

    signal evictDialogRequested()

    required property real localUsedStorage
    required property real remoteUsedStorage

    Layout.fillWidth: true
    columns: 3

    EnforcedPlainTextLabel {
        Layout.row: 0
        Layout.column: 0
        Layout.alignment: Layout.AlignLeft | Layout.AlignVCenter
        text: qsTr("Local storage use")
        font.bold: true
    }

    EnforcedPlainTextLabel {
        Layout.row: 0
        Layout.column: 1
        Layout.alignment: Layout.AlignRight | Layout.AlignVCenter
        Layout.fillWidth: true
        text: qsTr("%1 GB of %2 GB remote files synced").arg(root.localUsedStorage.toFixed(2)).arg(root.remoteUsedStorage.toFixed(2));
        elide: Text.ElideRight
        color: palette.dark
        horizontalAlignment: Text.AlignRight
    }

    Button {
        Layout.row: 0
        Layout.column: 2
        Layout.alignment: Layout.AlignRight | Layout.AlignVCenter
        text: qsTr("Evict local copies...")
        onPressed: root.evictDialogRequested()
    }

    ProgressBar {
        Layout.row: 1
        Layout.columnSpan: root.columns
        Layout.fillWidth: true
        value: root.localUsedStorage / root.remoteUsedStorage
    }
}

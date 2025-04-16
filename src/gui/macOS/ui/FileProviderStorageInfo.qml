/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
        text: qsTr("Free up space …")
        onPressed: root.evictDialogRequested()
    }

    ProgressBar {
        Layout.row: 1
        Layout.columnSpan: root.columns
        Layout.fillWidth: true
        value: root.localUsedStorage / root.remoteUsedStorage
    }
}

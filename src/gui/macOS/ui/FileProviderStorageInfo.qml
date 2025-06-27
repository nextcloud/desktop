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

ColumnLayout {
    id: root

    signal evictDialogRequested()

    required property real localUsedStorage
    required property real remoteUsedStorage

    Layout.fillWidth: true

    EnforcedPlainTextLabel {
        text: qsTr("Local storage use")
    }

    EnforcedPlainTextLabel {
        id: usageLabel
        text: qsTr("%1 GB of %2 GB remote files synced").arg(root.localUsedStorage.toFixed(2)).arg(root.remoteUsedStorage.toFixed(2))
        color: palette.dark
    }

    ProgressBar {
        Layout.preferredWidth: usageLabel.implicitWidth
        value: root.localUsedStorage / root.remoteUsedStorage
    }

    Button {
        text: qsTr("Free up space")
        onPressed: root.evictDialogRequested()
    }
}

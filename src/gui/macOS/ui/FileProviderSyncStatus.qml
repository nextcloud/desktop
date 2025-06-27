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

    signal domainSignalRequested
    required property var syncStatus

    NCBusyIndicator {
        id: syncIcon

        // reduce the icon size so the status row looks lighter
        property int size: Style.trayListItemIconSize * 0.56

        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
        
        padding: 0
        spacing: 0
        imageSource: root.syncStatus.icon
        running: root.syncStatus.syncing
    }

    EnforcedPlainTextLabel {
        Layout.fillWidth: true
        text: root.syncStatus.syncing ? qsTr("Syncing") : qsTr("All synced")
    }

    NCProgressBar {
        Layout.fillWidth: true
        value: root.syncStatus.fractionCompleted
        visible: root.syncStatus.syncing
    }

    Button {
        id: requestSyncButton
        text: qsTr("Request sync")
        visible: !root.syncStatus.syncing
        hoverEnabled: true
        onClicked: root.domainSignalRequested()

        ToolTip.visible: hovered
        ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        ToolTip.text: qsTr("Request a sync of changes for the VFS environment.\nmacOS may ignore or delay this request.")
    }
}

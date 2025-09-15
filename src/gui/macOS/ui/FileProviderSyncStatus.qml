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

    signal domainSignalRequested
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
        running: false // avoid rotating the icon when syncing
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

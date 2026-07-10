/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style

import com.nextcloud.desktopclient as NC
import "../../tray"

RowLayout {
    id: root

    property var syncStatusModel: null
    property var model: syncStatusModel ? syncStatusModel : defaultSyncStatus
    property color accentColor: Style.ncBlue
    property var user: null
    property var activityListModel: null

    spacing: Style.trayHorizontalMargin

    NC.SyncStatusSummary {
        id: defaultSyncStatus
    }

    NCBusyIndicator {
        id: syncIcon
        property int size: Style.trayListItemIconSize * 0.6
        property int whiteSpace: (Style.trayListItemIconSize - size)

        Layout.preferredWidth: size
        Layout.preferredHeight: size

        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        Layout.topMargin: Style.trayHorizontalMargin
        Layout.rightMargin: whiteSpace * (0.5 + Style.thumbnailImageSizeReduction)
        Layout.bottomMargin: Style.trayHorizontalMargin
        Layout.leftMargin: Style.trayHorizontalMargin + (whiteSpace * (0.5 - Style.thumbnailImageSizeReduction))

        padding: 0

        imageSource: root.model.syncIcon
        running: false // hotfix for download speed slowdown when tray is open
    }

    ColumnLayout {
        id: syncProgressLayout

        Layout.alignment: Qt.AlignVCenter
        Layout.topMargin: 8
        Layout.rightMargin: Style.trayHorizontalMargin
        Layout.bottomMargin: 8
        Layout.fillWidth: true
        Layout.fillHeight: true

        EnforcedPlainTextLabel {
            id: syncProgressText

            Layout.fillWidth: true

            text: root.model.syncStatusString
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: Style.topLinePixelSize
            font.bold: true
            wrapMode: Text.Wrap
        }

        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: Style.progressBarPreferredHeight

            active: root.model.syncing && root.model.totalFiles > 0
            visible: active

            sourceComponent: NCProgressBar {
                id: syncProgressBar
                value: root.model.syncProgress
                fillColor: root.accentColor
            }
        }

        EnforcedPlainTextLabel {
            id: syncProgressDetailText

            Layout.fillWidth: true

            text: root.model.syncStatusDetailString
            visible: root.model.syncStatusDetailString !== ""
            font.pixelSize: Style.subLinePixelSize
            wrapMode: Text.Wrap
        }
    }

    Button {
        id: syncNowButton

        Layout.rightMargin: Style.trayHorizontalMargin

        text: qsTr("Sync now")

        padding: Style.smallSpacing

        visible: root.user !== null &&
                 root.activityListModel !== null &&
                 !root.activityListModel.hasSyncConflicts &&
                 !root.model.syncing &&
                 !root.model.needsSandboxReapproval &&
                 (root.user.hasLocalFolder ||
                  (Qt.platform.os === "osx" && root.user.hasFileProvider)) &&
                 root.user.isConnected
        enabled: visible
        onClicked: {
            if(!root.model.syncing) {
                root.user.forceSyncNow();
            }
        }
    }

    Button {
        Layout.rightMargin: Style.trayHorizontalMargin

        text: qsTr("Resolve conflicts")

        visible: root.user !== null &&
                 root.activityListModel !== null &&
                 root.activityListModel.hasSyncConflicts &&
                 !root.model.syncing &&
                 root.user.hasLocalFolder &&
                 root.user.isConnected
        enabled: visible
        onClicked: NC.Systray.createResolveConflictsDialog(root.activityListModel.allConflicts);
    }

    Button {
        Layout.rightMargin: Style.trayHorizontalMargin

        text: qsTr("Open browser")

        visible: root.user !== null && root.user.needsToSignTermsOfService
        enabled: visible

        onClicked: root.user.openServer()
    }

    Button {
        Layout.rightMargin: Style.trayHorizontalMargin

        text: qsTr("Open settings")

        visible: root.model.needsSandboxReapproval &&
                 !root.model.syncing &&
                 root.user !== null &&
                 root.user.hasLocalFolder &&
                 root.user.isConnected
        enabled: visible

        onClicked: NC.Systray.openSettingsForSandboxReapproval()
    }
}

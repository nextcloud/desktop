/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

Page {
    id: root

    property string localPath: ""
    property string shortLocalPath: ""
    property string fileId: ""
    property Share selectedShare: null
    required property SharingController sharingController

    signal shareSelected(Share share)

    title: qsTr("Share \"%1\"").arg(root.shortLocalPath)
    background: Rectangle {
        color: palette.alternateBase
    }

    ColumnLayout {
        anchors.fill: parent

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: qsTr("Shares")
            font.bold: true
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.fillHeight: true

            text: qsTr("This item has not been shared yet.")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap

            visible: root.sharingController.shares.length === 0
        }

        ListView {
            id: sharesList

            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            spacing: Style.smallSpacing
            model: root.sharingController.shares
            visible: count > 0

            delegate: ShareEntry {
                required property Share modelData

                width: sharesList.width
                share: modelData
                selected: share === root.selectedShare

                onClicked: root.shareSelected(share)
            }
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: root.sharingController.shareCreationError
            color: Style.wizardErrorText
            wrapMode: Text.Wrap
            visible: text.length > 0
        }

        Button {
            Layout.fillWidth: true

            text: root.sharingController.creatingShare ? qsTr("Creating share…") : qsTr("Create share")
            enabled: !root.sharingController.creatingShare && root.fileId.length > 0

            onClicked: root.sharingController.createShare(root.fileId)
        }
    }
}

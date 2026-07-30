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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Style.standardSpacing

        RowLayout {
            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                Layout.fillWidth: true

                text: qsTr("Shares")
                font.bold: true
            }

            ToolButton {
                text: root.sharingController.creatingShare ? qsTr("Creating share…") : qsTr("Create share")
                display: AbstractButton.IconOnly
                icon.source: "image://svgimage-custom-color/add.svg/" + palette.buttonText
                enabled: !root.sharingController.creatingShare && root.fileId.length > 0

                ToolTip.visible: hovered
                ToolTip.text: text

                onClicked: root.sharingController.createShare(root.fileId)
            }
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
            currentIndex: {
                for (let row = 0; row < count; ++row) {
                    if (root.sharingController.shares[row] === root.selectedShare) {
                        return row
                    }
                }
                return -1
            }

            delegate: ShareEntry {
                required property int index
                required property Share modelData

                width: sharesList.width
                share: modelData
                highlighted: ListView.isCurrentItem

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

    }
}

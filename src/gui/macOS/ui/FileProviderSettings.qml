/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0
import "../../filedetails"
import "../../tray"

import com.nextcloud.desktopclient 1.0

Page {
    id: root

    property bool showBorder: false
    property var controller: FileProviderSettingsController
    property string accountUserIdAtHost: ""

    title: qsTr("Virtual files settings")

    background: Rectangle {
        color: palette.base
        border.width: root.showBorder ? Style.normalBorderWidth : 0
        border.color: root.palette.dark
    }

    padding: Style.standardSpacing

    ColumnLayout {
        id: rootColumn

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            text: qsTr("General settings")
            font.bold: true
            font.pointSize: Style.subheaderFontPtSize
            elide: Text.ElideRight
        }

        CheckBox {
            id: vfsEnabledCheckBox
            text: qsTr("Enable virtual files")
            checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
            onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)
        }
        
        CheckBox {
            id: trashDeletionCheckBox
            text: qsTr("Permanently delete files when removed from virtual drive")
            checked: root.controller.trashDeletionEnabledForAccount(root.accountUserIdAtHost)
            onClicked: root.controller.setTrashDeletionEnabledForAccount(root.accountUserIdAtHost, checked)

            Connections {
                target: root.controller
                function onTrashDeletionEnabledForAccountChanged(accountUserIdAtHost) {
                    if (root.accountUserIdAtHost !== accountUserIdAtHost) {
                        return;
                    }
                    trashDeletionCheckBox.checked = root.controller.trashDeletionEnabledForAccount(root.accountUserIdAtHost);
                }
            }
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            Layout.leftMargin: trashDeletionCheckBox.indicator.width + trashDeletionCheckBox.spacing
            visible: trashDeletionCheckBox.checked
            text: qsTr("⚠️ Warning: Deleted files will be permanently removed from the server and cannot be restored!")
            color: "#cc0000"
            wrapMode: Text.WordWrap
            font.italic: true
        }
    }
}

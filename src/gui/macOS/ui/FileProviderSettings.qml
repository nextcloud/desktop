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
            text: qsTr("Allow deletion of items in Trash")
            checked: root.controller.trashDeletionEnabledForAccount(root.accountUserIdAtHost)
            onClicked: root.controller.setTrashDeletionEnabledForAccount(root.accountUserIdAtHost, checked)
        }
    }
}

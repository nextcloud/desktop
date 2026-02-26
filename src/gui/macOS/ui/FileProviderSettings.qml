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
        color: palette.alternateBase
        border.width: root.showBorder ? Style.normalBorderWidth : 0
        border.color: palette.mid
    }

    leftPadding: 0
    rightPadding: 0
    topPadding: Style.standardSpacing
    bottomPadding: Style.standardSpacing
    // 1. Tell the Page how tall it actually is
    implicitHeight: rootColumn.implicitHeight + topPadding + bottomPadding

    ColumnLayout {
        id: rootColumn

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Style.standardSpacing

        RowLayout {
            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.WordWrap
                text: qsTr("Virtual files appear like regular files, but they do not use local storage space. The content downloads automatically when you open the file. Virtual files and classic sync can not be used at the same time.")
            }
            Switch {
                id: vfsEnabledCheckBox
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
                onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)
        }
    }
}

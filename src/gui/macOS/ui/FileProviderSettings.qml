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

        spacing: Style.standardSpacing

        CheckBox {
            id: vfsEnabledCheckBox
            text: qsTr("Enable virtual files")
            checked: root.controller.vfsEnabledForAccount(root.accountUserIdAtHost)
            enabled: !root.controller.isOperationInProgress
            onClicked: root.controller.setVfsEnabledForAccount(root.accountUserIdAtHost, checked)
        }

        RowLayout {
            spacing: Style.standardSpacing
            visible: root.controller.isOperationInProgress

            Item {
                Layout.preferredWidth: Style.standardSpacing
                Layout.preferredHeight: 1
            }

            NCBusyIndicator {
                id: operationIndicator
                running: root.controller.isOperationInProgress
                Layout.preferredWidth: Style.trayListItemIconSize * 0.6
                Layout.preferredHeight: Style.trayListItemIconSize * 0.6
            }

            EnforcedPlainTextLabel {
                text: root.controller.operationMessage
                Layout.leftMargin: Style.standardSpacing / 2
            }
        }
    }
}

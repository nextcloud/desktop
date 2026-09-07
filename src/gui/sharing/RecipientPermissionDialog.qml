/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui"
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

Dialog {
    id: root

    objectName: "recipientPermissionDialog"

    property SharingController sharingController: null
    property Share share: null
    property Recipient recipient: null
    property string updateError: ""
    property real availableWidth: Style.dialogWidth

    signal permissionToggled(string permissionClass, bool enabled)

    modal: true
    width: Math.min(Style.dialogWidth, root.availableWidth)
    padding: Style.standardSpacing
    title: root.recipient ? qsTr("Permissions for %1").arg(root.recipient.displayName) : qsTr("Recipient permissions")

    background: Rectangle {
        color: Style.wizardWindowBackground
        radius: Style.wizardDialogRadius
    }

    contentItem: ColumnLayout {
        spacing: Style.standardSpacing

        PermissionList {
            id: recipientPermissionList

            objectName: "recipientPermissionList"
            Layout.fillWidth: true
            model: PermissionModel {
                objectName: "recipientPermissionModel"
                share: root.share
                recipient: root.recipient
            }

            onPermissionToggled: (permissionClass, enabled) => {
                root.permissionToggled(permissionClass, enabled)
            }
        }

        ErrorBox {
            Layout.fillWidth: true
            text: root.updateError
            visible: text.length > 0
        }
    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Close
        onRejected: root.close()
    }
}

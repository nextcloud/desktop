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
    property real dialogAvailableWidth: Style.dialogWidth

    signal permissionToggled(string permissionClass, bool enabled)

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: Math.min(Style.dialogWidth, root.dialogAvailableWidth)
    padding: Style.wizardWindowMargin
    header: null
    footer: null

    background: Rectangle {
        objectName: "recipientPermissionDialogBackground"
        color: Style.wizardWindowBackground
        radius: Style.wizardDialogRadius
        border.width: Style.normalBorderWidth
        border.color: Style.wizardFieldBorder
    }

    contentItem: ColumnLayout {
        spacing: Style.wizardDialogSpacing

        EnforcedPlainTextLabel {
            objectName: "recipientPermissionDialogTitle"
            Layout.fillWidth: true
            text: root.recipient ? qsTr("Permissions for %1").arg(root.recipient.displayName) : qsTr("Recipient permissions")
            color: Style.wizardPrimaryText
            font.pixelSize: Style.wizardHeaderTitleFontPixelSize
            font.bold: true
            wrapMode: Text.WordWrap
        }

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

        RowLayout {
            Layout.fillWidth: true
            spacing: Style.wizardFooterSpacing

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                objectName: "closeRecipientPermissionDialogButton"
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}

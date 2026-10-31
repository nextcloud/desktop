/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls

import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

Dialog {
    signal deleteRequested
    signal cancelled

    modal: true
    title: qsTr("Delete share?")

    EnforcedPlainTextLabel {
        width: parent.width
        text: qsTr("This removes the share and its access for all recipients.")
        wrapMode: Text.Wrap
    }

    footer: DialogButtonBox {
        WizardButton {
            primary: true
            text: qsTr("Delete")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: accept()
        }

        WizardButton {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: reject()
        }
    }

    onAccepted: deleteRequested()
    onRejected: cancelled()
}

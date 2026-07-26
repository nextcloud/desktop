/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import "../../tray"

Dialog {
    id: root

    required property var controller
    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color hintTextColor: Style.wizardSecondaryText

    modal: true
    width: 420
    padding: 24
    header: null
    footer: null

    background: Rectangle {
        radius: 12
        color: Style.wizardWindowBackground
    }

    contentItem: ColumnLayout {
        spacing: 14
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Client certificate")

        EnforcedPlainTextLabel {
            text: qsTr("Client certificate")
            color: root.primaryTextColor
            font.pixelSize: Style.pixelSize + 8
            font.bold: true
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            text: qsTr("Select a PKCS#12 certificate file and enter its password.")
            color: root.hintTextColor
            font.pixelSize: Style.pixelSize + 1
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            WizardTextField {
                text: root.controller.clientCertificatePath
                placeholderText: qsTr("Certificate file")
                readOnly: true
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Choose")
                onClicked: root.controller.chooseClientCertificate()
            }
        }

        WizardTextField {
            text: root.controller.clientCertificatePassword
            placeholderText: qsTr("Certificate password")
            echoMode: TextInput.Password
            onTextEdited: root.controller.clientCertificatePassword = text
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            visible: root.controller.clientCertificateError !== ""
            text: root.controller.clientCertificateError
            color: Style.wizardErrorText
            font.pixelSize: Style.pixelSize + 1
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Cancel")
                onClicked: {
                    root.controller.clearClientCertificateInput()
                    root.close()
                }
            }

            WizardButton {
                primary: true
                enabled: root.controller.clientCertificateValid
                text: qsTr("Connect")
                onClicked: {
                    if (root.controller.submitClientCertificate()) {
                        root.close()
                    }
                }
            }
        }
    }
}

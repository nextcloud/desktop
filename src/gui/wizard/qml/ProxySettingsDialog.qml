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
    property int draftProxyMode: 0
    property int draftManualProxyType: 0
    property string draftProxyHost: ""
    property int draftProxyPort: 8080
    property bool draftProxyAuthenticationRequired: false
    property string draftProxyUser: ""
    property string draftProxyPassword: ""
    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color hintTextColor: Style.wizardSecondaryText
    readonly property bool draftProxySettingsValid: draftProxyMode !== 2
        || (draftProxyHost !== ""
            && (!draftProxyAuthenticationRequired || (draftProxyUser !== "" && draftProxyPassword !== "")))

    modal: true
    width: 420
    height: contentLayout.implicitHeight + topPadding + bottomPadding
    padding: 24
    header: null
    footer: null
    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Proxy settings")

    onOpened: loadFromController()

    function loadFromController() {
        draftProxyMode = root.controller.proxyMode
        draftManualProxyType = root.controller.manualProxyType
        draftProxyHost = root.controller.proxyHost
        draftProxyPort = root.controller.proxyPort
        draftProxyAuthenticationRequired = root.controller.proxyAuthenticationRequired
        draftProxyUser = root.controller.proxyUser
        draftProxyPassword = root.controller.proxyPassword
    }

    function applyToController() {
        root.controller.proxyMode = draftProxyMode
        root.controller.manualProxyType = draftManualProxyType
        root.controller.proxyHost = draftProxyHost
        root.controller.proxyPort = draftProxyPort
        root.controller.proxyAuthenticationRequired = draftProxyAuthenticationRequired
        root.controller.proxyUser = draftProxyUser
        root.controller.proxyPassword = draftProxyPassword
    }

    Behavior on height {
        NumberAnimation {
            duration: 160
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        radius: 12
        color: Style.wizardWindowBackground
    }

    contentItem: ColumnLayout {
        id: contentLayout

        spacing: 14

        EnforcedPlainTextLabel {
            text: qsTr("Proxy settings")
            color: root.primaryTextColor
            font.pixelSize: Style.pixelSize + 8
            font.bold: true
            Layout.fillWidth: true
        }

        RadioButton {
            text: qsTr("No proxy")
            checked: root.draftProxyMode === 0
            font.pixelSize: Style.pixelSize + 2
            Accessible.role: Accessible.RadioButton
            Accessible.name: text
            onClicked: root.draftProxyMode = 0
            Layout.fillWidth: true
        }

        RadioButton {
            text: qsTr("Use system proxy")
            checked: root.draftProxyMode === 1
            font.pixelSize: Style.pixelSize + 2
            Accessible.role: Accessible.RadioButton
            Accessible.name: text
            onClicked: root.draftProxyMode = 1
            Layout.fillWidth: true
        }

        RadioButton {
            text: qsTr("Manually specify proxy")
            checked: root.draftProxyMode === 2
            font.pixelSize: Style.pixelSize + 2
            Accessible.role: Accessible.RadioButton
            Accessible.name: text
            onClicked: root.draftProxyMode = 2
            Layout.fillWidth: true
        }

        ColumnLayout {
            visible: root.draftProxyMode === 2
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                model: [qsTr("HTTP(S) proxy"), qsTr("SOCKS5 proxy")]
                currentIndex: root.draftManualProxyType
                font.pixelSize: Style.pixelSize + 2
                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Proxy type")
                onActivated: root.draftManualProxyType = currentIndex
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                WizardTextField {
                    text: root.draftProxyHost
                    placeholderText: qsTr("Hostname of proxy server")
                    onTextEdited: root.draftProxyHost = text
                    Layout.fillWidth: true
                }

                SpinBox {
                    from: 1
                    to: 65535
                    value: root.draftProxyPort
                    editable: true
                    font.pixelSize: Style.pixelSize + 2
                    Accessible.role: Accessible.SpinBox
                    Accessible.name: qsTr("Proxy port")
                    onValueModified: root.draftProxyPort = value
                    textFromValue: function(value) { return value.toString() }
                    valueFromText: function(text) { return parseInt(text) }
                    Layout.preferredWidth: 110
                }
            }

            CheckBox {
                text: qsTr("Proxy server requires authentication")
                checked: root.draftProxyAuthenticationRequired
                font.pixelSize: Style.pixelSize + 2
                Accessible.role: Accessible.CheckBox
                Accessible.name: text
                onToggled: root.draftProxyAuthenticationRequired = checked
                Layout.fillWidth: true
            }

            RowLayout {
                visible: root.draftProxyAuthenticationRequired
                Layout.fillWidth: true
                spacing: 8

                WizardTextField {
                    text: root.draftProxyUser
                    placeholderText: qsTr("Username for proxy server")
                    onTextEdited: root.draftProxyUser = text
                    Layout.fillWidth: true
                }

                WizardTextField {
                    text: root.draftProxyPassword
                    placeholderText: qsTr("Password for proxy server")
                    echoMode: TextInput.Password
                    onTextEdited: root.draftProxyPassword = text
                    Layout.fillWidth: true
                }
            }

            EnforcedPlainTextLabel {
                visible: root.draftProxyMode === 2 && root.controller.showProxyLocalhostWarning
                text: qsTr("Note: proxy settings have no effects for accounts on localhost")
                color: root.hintTextColor
                font.pixelSize: Style.pixelSize
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            WizardButton {
                primary: true
                enabled: root.draftProxySettingsValid
                text: qsTr("Done")
                onClicked: {
                    root.applyToController()
                    root.close()
                }
            }
        }
    }
}

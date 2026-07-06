/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import com.nextcloud.desktopclient
import Style
import "../../tray"

import "../../tray"

ApplicationWindow {
    id: root

    property var controller
    property bool controllerFinished: false
    readonly property int compactHeight: 420
    readonly property int syncOptionsHeight: 520

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: 600
    height: compactHeight
    minimumWidth: 600
    minimumHeight: compactHeight
    title: ""
    // Explicit decoration set so macOS disables the green zoom/full-screen button (no maximize or
    // full-screen hint) — full screen makes no sense for this window — while keeping a normal,
    // draggable native title bar with close and minimize.
    flags: Qt.Window
        | Qt.CustomizeWindowHint
        | Qt.WindowTitleHint
        | Qt.WindowSystemMenuHint
        | Qt.WindowMinimizeButtonHint
        | Qt.WindowCloseButtonHint
    color: Style.wizardWindowBackground
    palette.window: Style.wizardWindowBackground
    palette.windowText: Style.wizardPrimaryText
    palette.base: Style.wizardFieldBackground
    palette.text: Style.wizardPrimaryText
    palette.button: Style.wizardFieldBackground
    palette.buttonText: Style.wizardPrimaryText
    palette.mid: Style.wizardDisabledText
    palette.placeholderText: Style.wizardPlaceholderText

    background: Rectangle {
        color: Style.wizardWindowBackground
    }

    function defaultHeightForCurrentStep() {
        return controller && controller.currentStep === AccountWizardController.SyncOptionsStep
            ? syncOptionsHeight
            : compactHeight
    }

    Component.onCompleted: {
        root.height = root.defaultHeightForCurrentStep()
        if (root.controller && root.controller.startLoginFlowAutomatically) {
            Qt.callLater(function() {
                root.controller.submitServerUrl()
            })
        }
    }

    onClosing: function(close) {
        if (!controllerFinished && controller) {
            controller.cancel()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: {
            if (root.controller) {
                root.controller.cancel()
            }
        }
    }

    Connections {
        target: controller

        function onFinished() {
            root.controllerFinished = true
            root.close()
        }

        function onAdvancedOptionsRequested() {
            advancedOptionsDialog.open()
        }

        function onProxySettingsRequested() {
            proxySettingsDialog.open()
        }

        function onClientCertificateDialogRequested() {
            clientCertificateDialog.open()
        }

        function onSecureConnectionFailed(host, retryHttpOnly) {
            secureConnectionFailureDialog.host = host
            secureConnectionFailureDialog.retryHttpOnly = retryHttpOnly
            secureConnectionFailureDialog.open()
        }

        function onCurrentStepChanged() {
            root.height = root.defaultHeightForCurrentStep()
        }
    }

    AdvancedOptionsDialog {
        id: advancedOptionsDialog
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        controller: root.controller
    }

    ProxySettingsDialog {
        id: proxySettingsDialog
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        controller: root.controller
    }

    ClientCertificateDialog {
        id: clientCertificateDialog
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        controller: root.controller
    }

    Dialog {
        id: secureConnectionFailureDialog

        property string host: ""
        property bool retryHttpOnly: false

        modal: true
        width: 420
        height: secureConnectionContent.implicitHeight + topPadding + bottomPadding
        padding: 24
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        header: null
        footer: null

        background: Rectangle {
            radius: 12
            color: Style.wizardWindowBackground
        }

        contentItem: ColumnLayout {
            id: secureConnectionContent

            spacing: 14
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Secure connection failed")

            EnforcedPlainTextLabel {
                text: qsTr("Connect to %1?").arg(secureConnectionFailureDialog.host)
                color: Style.wizardPrimaryText
                font.pixelSize: Style.pixelSize + 8
                font.bold: true
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            EnforcedPlainTextLabel {
                text: secureConnectionFailureDialog.retryHttpOnly
                    ? qsTr("The secure connection failed. You can retry without encryption, or add a client certificate and try again.")
                    : qsTr("The secure connection failed. You can add a client certificate and try again.")
                color: Style.wizardSecondaryText
                font.pixelSize: Style.pixelSize + 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                id: secureConnectionButtonColumn

                readonly property int buttonWidth: Math.min(secureConnectionContent.width,
                    Math.max(cancelButton.implicitWidth, withoutTlsButton.implicitWidth, clientCertificateButton.implicitWidth))

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                spacing: 8

                WizardButton {
                    id: cancelButton

                    primary: true
                    text: qsTr("Cancel")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: secureConnectionButtonColumn.buttonWidth
                    onClicked: secureConnectionFailureDialog.close()
                }

                WizardButton {
                    id: withoutTlsButton

                    visible: secureConnectionFailureDialog.retryHttpOnly
                    text: qsTr("Connect without TLS")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: secureConnectionButtonColumn.buttonWidth
                    onClicked: {
                        secureConnectionFailureDialog.close()
                        root.controller.retrySecureConnectionWithoutTls()
                    }
                }

                WizardButton {
                    id: clientCertificateButton

                    text: qsTr("Use client certificate")
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: secureConnectionButtonColumn.buttonWidth
                    onClicked: {
                        secureConnectionFailureDialog.close()
                        root.controller.useClientCertificateForSecureConnection()
                    }
                }
            }
        }
    }

    WizardDialogFrame {
        anchors.fill: parent

        Loader {
            anchors.fill: parent
            sourceComponent: {
                if (!root.controller) {
                    return null
                }
                switch (root.controller.currentStep) {
                case AccountWizardController.BrowserAuthStep:
                    return browserAuthPage
                case AccountWizardController.BasicAuthStep:
                    return basicAuthPage
                case AccountWizardController.SyncOptionsStep:
                    return syncOptionsPage
                default:
                    return serverPage
                }
            }
        }

        footer: [
            WizardButton {
                visible: root.controller && root.controller.currentStep !== AccountWizardController.ServerStep
                enabled: root.controller && !root.controller.busy
                Layout.fillWidth: root.controller
                    && (root.controller.currentStep === AccountWizardController.BrowserAuthStep
                        || root.controller.currentStep === AccountWizardController.BasicAuthStep
                        || root.controller.currentStep === AccountWizardController.SyncOptionsStep)
                Layout.preferredWidth: root.controller
                    && (root.controller.currentStep === AccountWizardController.BrowserAuthStep
                        || root.controller.currentStep === AccountWizardController.BasicAuthStep
                        || root.controller.currentStep === AccountWizardController.SyncOptionsStep)
                    ? 1
                    : implicitWidth
                text: root.controller && root.controller.currentStep === AccountWizardController.BrowserAuthStep
                    ? qsTr("Cancel")
                    : root.controller && root.controller.currentStep === AccountWizardController.SyncOptionsStep
                        ? qsTr("Cancel")
                        : qsTr("Back")
                onClicked: {
                    if (root.controller.currentStep === AccountWizardController.BrowserAuthStep
                            || root.controller.currentStep === AccountWizardController.SyncOptionsStep) {
                        root.controller.cancel()
                    } else {
                        root.controller.goBack()
                    }
                }
            },

            WizardButton {
                visible: root.controller && root.controller.currentStep === AccountWizardController.SyncOptionsStep
                enabled: root.controller && !root.controller.busy
                text: qsTr("Set up later")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.controller.skipFolderConfiguration()
            },

            WizardButton {
                visible: root.controller
                    && root.controller.currentStep === AccountWizardController.SyncOptionsStep
                    && root.controller.hasAdvancedOptions
                enabled: root.controller && !root.controller.busy
                text: qsTr("Advanced")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.controller.openAdvancedOptions()
            },

            WizardButton {
                visible: root.controller && root.controller.currentStep === AccountWizardController.ServerStep
                enabled: root.controller && !root.controller.busy
                text: qsTr("Sign up")
                textSuffix: "\u2197"
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.controller.openSignup()
            },

            WizardButton {
                visible: root.controller && root.controller.currentStep === AccountWizardController.ServerStep
                enabled: root.controller && !root.controller.busy
                text: qsTr("Self-host")
                textSuffix: "\u2197"
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.controller.openSelfHostedServerGuide()
            },

            Button {
                id: proxySettingsButton

                visible: root.controller
                    && root.controller.currentStep === AccountWizardController.ServerStep
                    && root.controller.proxySettingsAvailable
                enabled: root.controller && !root.controller.busy
                flat: true
                text: qsTr("Proxy settings")
                font.pixelSize: Style.pixelSize + 3
                font.weight: Font.Medium
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: 36
                onClicked: root.controller.openProxySettings()

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    enabled: proxySettingsButton.enabled
                    hoverEnabled: enabled
                    cursorShape: Qt.PointingHandCursor
                }
            },

            Item {
                visible: root.controller
                    && root.controller.currentStep !== AccountWizardController.ServerStep
                    && root.controller.currentStep !== AccountWizardController.BrowserAuthStep
                    && root.controller.currentStep !== AccountWizardController.BasicAuthStep
                    && root.controller.currentStep !== AccountWizardController.SyncOptionsStep
                Layout.fillWidth: visible
            },

            WizardButton {
                visible: root.controller && root.controller.currentStep === AccountWizardController.BrowserAuthStep
                enabled: root.controller && !root.controller.busy && root.controller.loginUrl.toString() !== ""
                text: qsTr("Copy link")
                iconSource: "image://svgimage-custom-color/copy.svg/" + palette.buttonText
                iconBeforeText: true
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.controller.copyLoginLink()
            },

            WizardButton {
                visible: root.controller && root.controller.currentStep !== AccountWizardController.ServerStep
                primary: true
                enabled: root.controller
                    && !root.controller.busy
                    && (root.controller.currentStep !== AccountWizardController.SyncOptionsStep
                        || root.controller.canFinish)
                    && (root.controller.currentStep !== AccountWizardController.BasicAuthStep
                        || root.controller.basicAuthValid)
                Layout.fillWidth: root.controller
                    && (root.controller.currentStep === AccountWizardController.BrowserAuthStep
                        || root.controller.currentStep === AccountWizardController.BasicAuthStep
                        || root.controller.currentStep === AccountWizardController.SyncOptionsStep)
                Layout.preferredWidth: root.controller
                    && (root.controller.currentStep === AccountWizardController.BrowserAuthStep
                        || root.controller.currentStep === AccountWizardController.BasicAuthStep
                        || root.controller.currentStep === AccountWizardController.SyncOptionsStep)
                    ? 1
                    : implicitWidth
                text: {
                    if (!root.controller) {
                        return ""
                    }
                    switch (root.controller.currentStep) {
                    case AccountWizardController.BrowserAuthStep:
                        return qsTr("Open")
                    case AccountWizardController.BasicAuthStep:
                        return qsTr("Connect")
                    case AccountWizardController.SyncOptionsStep:
                        return qsTr("Done")
                    default:
                        return qsTr("Log in")
                    }
                }
                textSuffix: root.controller && root.controller.currentStep === AccountWizardController.BrowserAuthStep
                    ? "\u2197"
                    : ""
                onClicked: {
                    switch (root.controller.currentStep) {
                    case AccountWizardController.BrowserAuthStep:
                        root.controller.openBrowserLogin()
                        break
                    case AccountWizardController.BasicAuthStep:
                        root.controller.submitBasicAuth()
                        break
                    case AccountWizardController.SyncOptionsStep:
                        root.controller.finish()
                        break
                    default:
                        root.controller.submitServerUrl()
                    }
                }
            }
        ]
    }

    Component {
        id: serverPage
        ServerPage {
            controller: root.controller
        }
    }

    Component {
        id: browserAuthPage
        BrowserAuthPage {
            controller: root.controller
        }
    }

    Component {
        id: basicAuthPage
        BasicAuthPage {
            controller: root.controller
        }
    }

    Component {
        id: syncOptionsPage
        SyncOptionsPage {
            controller: root.controller
        }
    }
}

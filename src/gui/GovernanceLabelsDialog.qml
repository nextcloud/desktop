/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Window as QtWindow
import QtQuick.Layouts
import QtQuick.Controls
import QtQml.Models
import Style
import com.nextcloud.desktopclient
import "./tray"
import "./wizard/qml"

ApplicationWindow {
    id: governanceLabelsDialog

    required property var fileName
    required property var fileId
    required property var account

    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color hintTextColor: Style.wizardSecondaryText
    readonly property color networkErrorTextColor: Style.wizardErrorText

    property string lastError: ''

    function displayError(networkError) {
        lastError = networkError
    }

    function clearError() {
        lastError = ''
    }

    flags: Qt.Window | Qt.Dialog
    visible: true

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    minimumWidth: mainContent.implicitWidth
    minimumHeight: mainContent.implicitHeight + 40
    width: Style.defaultWidthGovernanceLabelsDialog
    height: Style.defaultHeightGovernanceLabelsDialog

    title: qsTr("Apply labels")

    color: Style.wizardWindowBackground
    palette.window:          Style.wizardWindowBackground
    palette.base:            Style.wizardFieldBackground
    palette.button:          Style.wizardFieldBackground
    palette.mid:             Style.wizardDisabledText
    palette.placeholderText: Style.wizardPlaceholderText
    palette.active.windowText:   Style.wizardPrimaryText
    palette.inactive.windowText: Style.wizardPrimaryText
    palette.disabled.windowText: Style.wizardDisabledText
    palette.active.text:   Style.wizardPrimaryText
    palette.inactive.text: Style.wizardPrimaryText
    palette.disabled.text: Style.wizardDisabledText
    palette.active.buttonText:   Style.wizardPrimaryText
    palette.inactive.buttonText: Style.wizardPrimaryText
    palette.disabled.buttonText: Style.wizardDisabledText

    background: Rectangle {
        color: Style.wizardWindowBackground
    }

    onClosing: function(close) {
        Systray.destroyDialog(governanceLabelsDialog);
        close.accepted = true
    }

    GovernanceLabelsListModel {
        id: sensitivityLabelsModel

        account: governanceLabelsDialog.account
        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Sensitivity
        labelBehavior: GovernanceLabelsListModel.UniqueLabel

        onRefreshAvailableLabelsData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForSensitivity.start(labelType, entityId)
        }

        onDisplayError: function(errorMessage) {
            governanceLabelsDialog.displayError(errorMessage)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForSensitivity

        account: governanceLabelsDialog.account

        onStarted: function() {
            clearError()
        }

        onFinished: function(reply) {
            sensitivityLabelsModel.setAvailableLabelsJsonData(reply)
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    GovernanceLabelsListModel {
        id: retentionLabelsModel

        account: governanceLabelsDialog.account
        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Retention
        labelBehavior: GovernanceLabelsListModel.MultipleLabels

        onRefreshAvailableLabelsData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForRetention.start(labelType, entityId)
        }

        onDisplayError: function(errorMessage) {
            governanceLabelsDialog.displayError(errorMessage)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForRetention

        account: governanceLabelsDialog.account

        onStarted: function() {
            clearError()
        }

        onFinished: function(reply) {
            retentionLabelsModel.setAvailableLabelsJsonData(reply)
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    GovernanceLabelsListModel {
        id: legalHoldLabelsModel

        account: governanceLabelsDialog.account
        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.LegalHold
        labelBehavior: GovernanceLabelsListModel.MultipleLabels

        onRefreshAvailableLabelsData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForLegalHold.start(labelType, entityId)
        }

        onDisplayError: function(errorMessage) {
            governanceLabelsDialog.displayError(errorMessage)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForLegalHold

        account: governanceLabelsDialog.account

        onStarted: function() {
            clearError()
        }

        onFinished: function(reply) {
            legalHoldLabelsModel.setAvailableLabelsJsonData(reply)
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    ColumnLayout {
        id: mainContent

        enabled: !sensitivityLabelsModel.busy && !retentionLabelsModel.busy && !legalHoldLabelsModel.busy

        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.bottomMargin: 24
        anchors.topMargin: 24
        spacing: 14

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: qsTr("Assign labels to the file to manage its sensitivity, retention, and legal hold policies.")
            color: governanceLabelsDialog.hintTextColor
            font.pixelSize: Style.headerFontPtSize
            wrapMode: Text.WordWrap
            // width: governanceLabelsDialog.width - 48
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: lastError
            color: governanceLabelsDialog.networkErrorTextColor
            font.pixelSize: Style.headerFontPtSize
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: qsTr("Sensitivity labels")
            color: governanceLabelsDialog.hintTextColor
            font.pixelSize: Style.headerFontPtSize
            Layout.alignment: Qt.AlignVCenter
        }

        WizardComboBox {
            id: selectedNewSensitivityLabel

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Select sensitivity label")

            enabled: !sensitivityLabelsModel.isEmpty
            model: sensitivityLabelsModel
            textRole: "name"
            valueRole: "id"

            Layout.fillWidth: true

            onActivated: function(index) {
                sensitivityLabelsModel.toggleLabel(index)
            }
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: qsTr("Retention labels")
            color: governanceLabelsDialog.hintTextColor
            font.pixelSize: Style.headerFontPtSize
            Layout.alignment: Qt.AlignVCenter
        }

        WizardComboBox {
            id: selectedNewRetentionLabel

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Select retention label")

            enabled: !retentionLabelsModel.isEmpty
            model: retentionLabelsModel
            textRole: "name"
            valueRole: "id"

            Layout.fillWidth: true

            onActivated: function(index) {
                retentionLabelsModel.toggleLabel(index)
            }
        }

        EnforcedPlainTextLabel {
            Layout.fillWidth: true

            text: qsTr("Legal hold labels")
            color: governanceLabelsDialog.hintTextColor
            font.pixelSize: Style.headerFontPtSize
        }

        WizardComboBox {
            id: selectedNewLegalHoldLabel

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Select legal hold label")

            enabled: !legalHoldLabelsModel.isEmpty
            model: legalHoldLabelsModel
            textRole: "name"
            valueRole: "id"

            Layout.fillWidth: true

            onActivated: function(index) {
                legalHoldLabelsModel.toggleLabel(index)
            }
        }

        Item {
            Layout.fillHeight: true
        }

        DialogButtonBox {
            id: buttonBox

            Layout.fillWidth: true

            enabled: !sensitivityLabelsModel.busy && !retentionLabelsModel.busy && !legalHoldLabelsModel.busy &&
                     (sensitivityLabelsModel.hasPendingChanges || retentionLabelsModel.hasPendingChanges || legalHoldLabelsModel.hasPendingChanges)

            onReset: function() {
                sensitivityLabelsModel.reset()
                retentionLabelsModel.reset()
                legalHoldLabelsModel.reset()
            }

            onAccepted: function() {
                sensitivityLabelsModel.apply()
                retentionLabelsModel.apply()
                legalHoldLabelsModel.apply()
            }

            WizardButton {
                text: qsTr("Reset")
                DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
            }

            WizardButton {
                text: qsTr("Apply")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }
}

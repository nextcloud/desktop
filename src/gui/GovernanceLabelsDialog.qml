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
    palette.windowText:      Style.wizardPrimaryText
    palette.base:            Style.wizardFieldBackground
    palette.text:            Style.wizardPrimaryText
    palette.button:          Style.wizardFieldBackground
    palette.buttonText:      Style.wizardPrimaryText
    palette.mid:             Style.wizardDisabledText
    palette.placeholderText: Style.wizardPlaceholderText

    background: Rectangle {
        color: Style.wizardWindowBackground
    }

    onClosing: function(close) {
        Systray.destroyDialog(self);
        close.accepted = true
    }

    GetGovernanceLabels {
        id: getGovernanceLabels

        account: governanceLabelsDialog.account

        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function(reply) {
            sensitivityLabelsModel.setExistingLabelsJsonData(reply)
            retentionLabelsModel.setExistingLabelsJsonData(reply)
            legalHoldLabelsModel.setExistingLabelsJsonData(reply)
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    GovernanceLabelsListModel {
        id: sensitivityLabelsModel

        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Sensitivity
        labelBehavior: GovernanceLabelsListModel.UniqueLabel

        onRefreshAvailableLabelsData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForSensitivity.start(labelType, entityId)
        }

        onRefreshExistingLabelsData: function(labelType, entityId) {
            getGovernanceLabels.start(entityId)
        }

        onAddLabel: function(labelId) {
            applySensitivityLabel.start(labelId)
        }

        onRemoveLabel: function(labelId) {
            deleteSensitivityLabel.start(labelId)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForSensitivity

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId

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

    ApplyGovernanceLabel {
        id: applySensitivityLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function() {
            sensitivityLabelsModel.labelWasModified()
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    DeleteGovernanceLabel {
        id: deleteSensitivityLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function() {
            sensitivityLabelsModel.labelWasModified()
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    GovernanceLabelsListModel {
        id: retentionLabelsModel

        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Retention
        labelBehavior: GovernanceLabelsListModel.MultipleLabels

        onRefreshAvailableLabelsData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForSensitivity.start(labelType, entityId)
        }

        onRefreshExistingLabelsData: function(labelType, entityId) {
            getGovernanceLabels.start(entityId)
        }

        onAddLabel: function(labelId) {
            applyRetentionLabel.start(labelId)
        }

        onRemoveLabel: function(labelId) {
            deleteRetentionLabel.start(labelId)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForRetention

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Retention
        entityId: governanceLabelsDialog.fileId

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

    ApplyGovernanceLabel {
        id: applyRetentionLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Retention
        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function() {
            retentionLabelsModel.labelWasModified()
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    DeleteGovernanceLabel {
        id: deleteRetentionLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Retention
        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function() {
            retentionLabelsModel.labelWasModified()
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    GovernanceLabelsListModel {
        id: legalHoldLabelsModel

        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.LegalHold
        labelBehavior: GovernanceLabelsListModel.MultipleLabels

        onRefreshAvailableLabelsData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForLegalHold.start(labelType, entityId)
        }

        onRefreshExistingLabelsData: function(labelType, entityId) {
            getGovernanceLabels.start(entityId)
        }

        onAddLabel: function(labelId) {
            applyLegalHoldLabel.start(labelId)
        }

        onRemoveLabel: function(labelId) {
            deleteLegalHoldLabel.start(labelId)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForLegalHold

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.LegalHold
        entityId: governanceLabelsDialog.fileId

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

    ApplyGovernanceLabel {
        id: applyLegalHoldLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.LegalHold
        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function() {
            legalHoldLabelsModel.labelWasModified()
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    DeleteGovernanceLabel {
        id: deleteLegalHoldLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.LegalHold
        entityId: governanceLabelsDialog.fileId

        onStarted: function() {
            clearError()
        }

        onFinished: function() {
            legalHoldLabelsModel.labelWasModified()
        }

        onFinishedWithError: function(errorCode, errorMessage) {
            displayError(errorMessage)
        }
    }

    ColumnLayout {
        id: mainContent

        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.bottomMargin: 24
        anchors.topMargin: 24
        spacing: 14

        EnforcedPlainTextLabel {
            text: lastError
            color: governanceLabelsDialog.networkErrorTextColor
            font.pixelSize: Style.pixelSize
        }

        RowLayout {
            spacing: 4

            Image {
                source: "image://svgimage-custom-color/security.svg/" + governanceLabelsDialog.hintTextColor
                sourceSize.width: Style.smallIconSize
                sourceSize.height: Style.smallIconSize
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignVCenter
                opacity: governanceLabelsDialog.hintTextColor.a
            }

            EnforcedPlainTextLabel {
                text: qsTr("Sensitivity:")
                color: governanceLabelsDialog.hintTextColor
                font.pixelSize: Style.pixelSize
                Layout.alignment: Qt.AlignVCenter
            }
        }

        WizardComboBox {
            id: selectedNewSensitivityLabel

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Select sensitivity label")

            model: sensitivityLabelsModel
            textRole: "name"
            valueRole: "id"

            Layout.fillWidth: true

            onActivated: function(index) {
                sensitivityLabelsModel.toggleLabel(index, currentValue)
            }
        }

        RowLayout {
            spacing: 4

            Image {
                source: "image://svgimage-custom-color/file-clock-outline.svg/" + governanceLabelsDialog.hintTextColor
                sourceSize.width: Style.smallIconSize
                sourceSize.height: Style.smallIconSize
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignVCenter
                opacity: governanceLabelsDialog.hintTextColor.a
            }

            EnforcedPlainTextLabel {
                text: qsTr("Retention:")
                color: governanceLabelsDialog.hintTextColor
                font.pixelSize: Style.pixelSize
                Layout.alignment: Qt.AlignVCenter
            }
        }

        WizardComboBox {
            id: selectedNewRetentionLabel

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Select retention label")

            model: retentionLabelsModel
            textRole: "name"
            valueRole: "id"

            Layout.fillWidth: true

            onActivated: function() {
                retentionLabelsModel.toggleLabel(currentValue)
            }
        }

        EnforcedPlainTextLabel {
            text: qsTr("Legal hold:")
            color: governanceLabelsDialog.hintTextColor
            font.pixelSize: Style.pixelSize
        }

        WizardComboBox {
            id: selectedNewLegalHoldLabel

            Accessible.role: Accessible.ComboBox
            Accessible.name: qsTr("Select legal hold label")

            model: legalHoldLabelsModel
            textRole: "name"
            valueRole: "id"

            Layout.fillWidth: true

            onActivated: function() {
                legalHoldLabelsModel.toggleLabel(currentValue)
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Done")
                onClicked: Systray.destroyDialog(governanceLabelsDialog)
            }
        }
    }
}

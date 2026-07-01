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

    flags: Qt.Window | Qt.Dialog
    visible: true

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: Style.minimumWidthResolveConflictsDialog
    height: Style.minimumHeightResolveConflictsDialog
    minimumWidth: Style.minimumWidthResolveConflictsDialog
    minimumHeight: Style.minimumHeightResolveConflictsDialog
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

    GovernanceLabelsListModel {
        id: sensitivityLabelsModel

        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Sensitivity

        onRefreshData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForSensitivity.start(labelType, entityId)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForSensitivity

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId

        onFinished: function(reply) {
            sensitivityLabelsModel.setAvailableLabelsJsonData(reply)
        }
    }

    ApplyGovernanceLabel {
        id: applySensitivityLabel

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId

        onFinished: function(reply) {
            sensitivityLabelsModel.setAvailableLabelsJsonData(reply)
        }
    }

    GovernanceLabelsListModel {
        id: retentionLabelsModel

        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Retention

        onRefreshData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForSensitivity.start(labelType, entityId)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForRetention

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Retention
        entityId: governanceLabelsDialog.fileId

        onFinished: function(reply) {
            retentionLabelsModel.setAvailableLabelsJsonData(reply)
        }
    }

    GovernanceLabelsListModel {
        id: legalHoldLabelsModel

        entityId: governanceLabelsDialog.fileId
        labelType: GovernanceNetworkJob.Retention

        onRefreshData: function(labelType, entityId) {
            getAvailableGovernanceLabelsForSensitivity.start(labelType, entityId)
        }
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForLegalHold

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Retention
        entityId: governanceLabelsDialog.fileId

        onFinished: function(reply) {
            legalHoldLabelsModel.setAvailableLabelsJsonData(reply)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.bottomMargin: 24
        anchors.topMargin: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            EnforcedPlainTextLabel {
                text: qsTr("Sensitivity:")
                color: governanceLabelsDialog.hintTextColor
                font.pixelSize: Style.pixelSize
            }

            WizardComboBox {
                id: selectedNewSensitivityLabel

                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Select sensitivity label")

                model: sensitivityLabelsModel
                textRole: "name"
                valueRole: "id"

                Layout.fillWidth: true

                onActivated: function() {
                    applySensitivityLabel.labelId = currentValue
                    applySensitivityLabel.start()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            EnforcedPlainTextLabel {
                text: qsTr("Retention:")
                color: governanceLabelsDialog.hintTextColor
                font.pixelSize: Style.pixelSize
            }

            WizardComboBox {
                id: selectedNewRetentionLabel

                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Select retention label")

                model: retentionLabelsModel
                textRole: "name"
                valueRole: "id"
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

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
                text: qsTr("Cancel")
                onClicked: Systray.destroyDialog(governanceLabelsDialog)
            }
        }
    }
}

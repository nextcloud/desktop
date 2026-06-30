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

ApplicationWindow {
    id: governanceLabelsDialog

    required property var fileName
    required property var fileId
    required property var account

    flags: Qt.Window | Qt.Dialog
    visible: true

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: Style.minimumWidthResolveConflictsDialog
    height: Style.minimumHeightResolveConflictsDialog
    minimumWidth: Style.minimumWidthResolveConflictsDialog
    minimumHeight: Style.minimumHeightResolveConflictsDialog
    title: qsTr('Applys labels')

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
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 5
        anchors.topMargin: 10
        spacing: 15
        z: 2

        RowLayout {
            EnforcedPlainTextLabel {
                text: 'Sensitivity label:'

                font.pixelSize: Style.pixelSize + 2
            }

            ComboBox {
                id: selectedNewSensitivityLabel

                font.pixelSize: Style.pixelSize + 2
                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Select sensitivity label")

                model: sensitivityLabelsModel
                textRole: 'name'
                valueRole: 'id'
            }
        }

        RowLayout {
            EnforcedPlainTextLabel {
                text: 'Retention labels:'

                font.pixelSize: Style.pixelSize + 2
            }

            ComboBox {
                id: selectedNewRetentionLabel

                font.pixelSize: Style.pixelSize + 2
                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Select retention labels")

                model: retentionLabelsModel
                textRole: 'name'
                valueRole: 'id'
            }
        }

        RowLayout {
            EnforcedPlainTextLabel {
                text: 'Legal hold labels:'

                font.pixelSize: Style.pixelSize + 2
            }

            ComboBox {
                id: selectedNewLegalHoldLabel

                font.pixelSize: Style.pixelSize + 2
                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Select legal hold labels")

                model: legalHoldLabelsModel
                textRole: 'name'
                valueRole: 'id'
            }
        }

        DialogButtonBox {
            Layout.fillWidth: true

            Button {
                text: qsTr("Apply")
                DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            }

            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            onAccepted: function() {
                Systray.destroyDialog(governanceLabelsDialog)
            }

            onRejected: function() {
                Systray.destroyDialog(governanceLabelsDialog)
            }
        }
    }

    Rectangle {
        color: palette.base
        anchors.fill: parent
        z: 1
    }
}

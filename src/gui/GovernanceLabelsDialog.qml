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

    ApplyGovernanceLabel {
        id: applyGovernanceLabel

        account: governanceLabelsDialog.account

        labelId: 'labelId'
        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId
    }

    DeleteGovernanceLabel {
        id: deleteGovernanceLabel

        account: governanceLabelsDialog.account

        labelId: 'labelId'
        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForSensitivity

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Sensitivity
        entityId: governanceLabelsDialog.fileId
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForHold

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Hold
        entityId: governanceLabelsDialog.fileId
    }

    GetAvailableGovernanceLabels {
        id: getAvailableGovernanceLabelsForRetention

        account: governanceLabelsDialog.account

        labelType: GovernanceNetworkJob.Retention
        entityId: governanceLabelsDialog.fileId
    }

    GetGovernanceLabels {
        id: getGovernanceLabels

        account: governanceLabelsDialog.account

        entityId: governanceLabelsDialog.fileId
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 5
        anchors.topMargin: 10
        spacing: 15
        z: 2

        Button {
            text: 'Apply governance label'
            onClicked: applyGovernanceLabel.start()
        }

        Button {
            text: 'Delete governance label'
            onClicked: deleteGovernanceLabel.start()
        }

        Button {
            text: 'Get available governance labels for sensitivity'
            onClicked: getAvailableGovernanceLabelsForSensitivity.start()
        }

        Button {
            text: 'Get available governance labels for retention'
            onClicked: getAvailableGovernanceLabelsForRetention.start()
        }

        Button {
            text: 'Get available governance labels for legal hold'
            onClicked: getAvailableGovernanceLabelsForHold.start()
        }

        Button {
            text: 'Get governance labels'
            onClicked: getGovernanceLabels.start()
        }

        DialogButtonBox {
            Layout.fillWidth: true

            Button {
                text: qsTr("Close")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
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

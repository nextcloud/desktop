/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"
import "../../wizard/qml"

Rectangle {
    id: root

    required property NC.AssistantController assistantController
    required property double taskId
    required property string input
    required property string output
    required property string statusText
    required property string dateText

    readonly property string statusSummary: root.dateText.length > 0
        ? qsTr("%1 · %2").arg(root.statusText, root.dateText)
        : root.statusText

    signal deleteRequested(double taskId)

    width: ListView.view ? ListView.view.width : 0
    implicitHeight: taskColumn.implicitHeight + Style.standardSpacing * 2
    radius: Style.wizardDialogRadius
    color: Style.wizardRowBackground
    border.width: Style.normalBorderWidth
    border.color: Style.wizardFieldBorder

    ColumnLayout {
        id: taskColumn

        anchors.fill: parent
        anchors.margins: Style.standardSpacing
        spacing: Style.wizardFooterSpacing

        RowLayout {
            spacing: Style.wizardFooterSpacing
            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                text: root.statusSummary
                color: Style.wizardSecondaryText
                font.pixelSize: Style.pixelSize
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Retry")
                enabled: !root.assistantController.requestInProgress
                onClicked: root.assistantController.retryTask(root.taskId)
            }

            WizardButton {
                text: qsTr("Delete")
                enabled: !root.assistantController.requestInProgress
                onClicked: root.deleteRequested(root.taskId)
            }
        }

        TextEdit {
            text: root.input
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: Style.wizardPrimaryText
            selectedTextColor: Style.wizardSelectedText
            selectionColor: Style.ncBlue
            textFormat: Text.PlainText
            readOnly: true
            selectByMouse: true
            Layout.fillWidth: true
        }

        TextEdit {
            visible: root.output.length > 0
            text: root.output
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: Style.wizardSecondaryText
            selectedTextColor: Style.wizardSelectedText
            selectionColor: Style.ncBlue
            textFormat: Text.MarkdownText
            readOnly: true
            selectByMouse: true
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
        }
    }
}

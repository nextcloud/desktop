/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import com.nextcloud.desktopclient as NC
import Style
import "../../tray"
import "../../wizard/qml"

Dialog {
    id: root
    objectName: "assistantDeleteTaskDialog"

    required property NC.AssistantController assistantController

    property double taskId: -1

    modal: true
    header: null
    footer: null

    background: Rectangle {
        radius: Style.wizardDialogRadius
        color: Style.wizardWindowBackground
        border.width: Style.normalBorderWidth
        border.color: Style.wizardFieldBorder
    }

    contentItem: ColumnLayout {
        spacing: Style.wizardDialogSpacing
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Delete assistant task?")

        EnforcedPlainTextLabel {
            text: qsTr("Delete assistant task?")
            color: Style.wizardPrimaryText
            font.pixelSize: Style.wizardHeaderTitleFontPixelSize
            font.bold: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            text: qsTr("This removes the task from the server.")
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Style.wizardFooterSpacing
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                objectName: "assistantDeleteTaskCancelButton"
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            WizardButton {
                objectName: "assistantDeleteTaskConfirmButton"
                primary: true
                text: qsTr("Delete")
                onClicked: {
                    root.assistantController.deleteTask(root.taskId)
                    root.close()
                }
            }
        }
    }
}

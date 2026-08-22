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

ColumnLayout {
    id: root

    required property NC.AssistantController assistantController

    spacing: Style.wizardSectionSpacing

    EnforcedPlainTextLabel {
        visible: root.assistantController.selectedTaskTypeDescription.length > 0
        text: visible ? root.assistantController.selectedTaskTypeDescription : ""
        color: Style.wizardSecondaryText
        font.pixelSize: Style.wizardBodyFontPixelSize
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? implicitHeight : 0
    }

    ListView {
        id: taskList

        clip: true
        spacing: Style.wizardSectionSpacing
        boundsBehavior: Flickable.StopAtBounds
        model: root.assistantController.tasks
        Layout.fillWidth: true
        Layout.fillHeight: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Rectangle {
            id: taskDelegate

            required property double taskId
            required property string input
            required property string output
            required property string statusText
            required property string dateText

            readonly property string statusSummary: dateText.length > 0
                ? qsTr("%1 · %2").arg(statusText, dateText)
                : statusText

            width: taskList.width
            implicitHeight: taskColumn.implicitHeight + 20
            radius: Style.wizardDialogRadius
            color: Style.wizardRowBackground
            border.width: Style.normalBorderWidth
            border.color: Style.wizardFieldBorder

            ColumnLayout {
                id: taskColumn

                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    spacing: Style.wizardFooterSpacing
                    Layout.fillWidth: true

                    EnforcedPlainTextLabel {
                        text: taskDelegate.statusSummary
                        color: Style.wizardSecondaryText
                        font.pixelSize: Style.pixelSize
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    WizardButton {
                        text: qsTr("Retry")
                        enabled: !root.assistantController.requestInProgress
                        onClicked: root.assistantController.retryTask(taskDelegate.taskId)
                    }

                    WizardButton {
                        text: qsTr("Delete")
                        enabled: !root.assistantController.requestInProgress
                        onClicked: {
                            deleteTaskDialog.taskId = taskDelegate.taskId
                            deleteTaskDialog.open()
                        }
                    }
                }

                TextEdit {
                    text: taskDelegate.input
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
                    visible: taskDelegate.output.length > 0
                    text: taskDelegate.output
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

        EnforcedPlainTextLabel {
            anchors.centerIn: parent
            width: Math.min(parent.width, 360)
            visible: taskList.count === 0 && !root.assistantController.requestInProgress
            text: qsTr("No assistant tasks for this type.")
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    Dialog {
        id: deleteTaskDialog

        property double taskId: -1

        modal: true
        width: Math.min(Style.wizardDialogMaximumWidth, root.width - Style.wizardWindowMargin * 2)
        padding: Style.wizardWindowMargin
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
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
                    text: qsTr("Cancel")
                    onClicked: deleteTaskDialog.close()
                }

                WizardButton {
                    primary: true
                    text: qsTr("Delete")
                    onClicked: {
                        root.assistantController.deleteTask(deleteTaskDialog.taskId)
                        deleteTaskDialog.close()
                    }
                }
            }
        }
    }
}

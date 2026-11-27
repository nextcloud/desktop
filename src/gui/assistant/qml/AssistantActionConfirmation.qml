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

Pane {
    id: root
    objectName: "assistantActionConfirmation"

    required property NC.AssistantController assistantController
    readonly property var actions: assistantController.pendingActions
    readonly property bool canAnswer: !assistantController.requestInProgress
        && assistantController.accountConnected && assistantController.assistantEnabled
    readonly property bool detailsAvailable: actions.length > 0 && actions.every(action =>
        action !== null && typeof action === "object" && typeof action.name === "string"
        && action.name.length > 0 && action.args !== null && typeof action.args === "object"
        && !Array.isArray(action.args))

    padding: Style.standardSpacing

    background: Rectangle {
        color: Style.assistantConfirmationBackground
        radius: Style.mediumRoundedButtonRadius

        Rectangle {
            width: Style.assistantConfirmationAccentWidth
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: Style.wizardRadioAccent
            radius: Style.assistantConfirmationAccentWidth / 2
        }
    }

    contentItem: ColumnLayout {
        spacing: Style.smallSpacing

        EnforcedPlainTextLabel {
            text: qsTr("The Assistant wants to perform sensitive actions on your behalf.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ScrollView {
            id: detailsScroll
            objectName: "assistantActionDetailsScroll"
            clip: true
            contentWidth: availableWidth
            contentHeight: details.implicitHeight
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(details.implicitHeight, Style.assistantActionDetailsMaximumHeight)

            ColumnLayout {
                id: details
                width: detailsScroll.availableWidth
                spacing: Style.standardSpacing

                Repeater {
                    model: root.actions
                    delegate: AssistantActionDetails {
                        required property var modelData
                        action: modelData
                        Layout.fillWidth: true
                    }
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Style.wizardFooterSpacing

            WizardButton {
                objectName: "assistantCancelActionsButton"
                text: qsTr("Cancel")
                enabled: root.canAnswer
                onClicked: root.assistantController.answerPendingActions(false)
            }
            WizardButton {
                objectName: "assistantConfirmActionsButton"
                text: qsTr("Confirm actions")
                primary: true
                enabled: root.canAnswer && root.detailsAvailable
                onClicked: root.assistantController.answerPendingActions(true)
            }
        }
    }
}

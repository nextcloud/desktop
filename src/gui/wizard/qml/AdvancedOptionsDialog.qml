/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import "../../tray"

Dialog {
    id: root

    required property var controller
    readonly property color primaryTextColor: Style.wizardPrimaryText

    modal: true
    width: 360
    padding: 24
    header: null
    footer: null
    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Advanced options")

    background: Rectangle {
        radius: 12
        color: Style.wizardWindowBackground
    }

    contentItem: ColumnLayout {
        spacing: 14

        EnforcedPlainTextLabel {
            text: qsTr("Advanced options")
            color: root.primaryTextColor
            font.pixelSize: Style.pixelSize + 8
            font.bold: true
            Layout.fillWidth: true
        }

        CheckBox {
            visible: root.controller.showLargeFolderConfirmation
            text: qsTr("Ask before syncing folders larger than")
            checked: root.controller.askBeforeLargeFolders
            font.pixelSize: Style.pixelSize + 2
            Accessible.role: Accessible.CheckBox
            Accessible.name: text
            onToggled: root.controller.setAskBeforeLargeFolders(checked)
            Layout.fillWidth: true
        }

        SpinBox {
            visible: root.controller.showLargeFolderConfirmation
            from: 0
            to: 1048576
            value: root.controller.largeFolderThresholdMb
            enabled: root.controller.askBeforeLargeFolders
            editable: true
            font.pixelSize: Style.pixelSize + 2
            Accessible.role: Accessible.SpinBox
            Accessible.name: qsTr("Large folder threshold")
            onValueModified: root.controller.setLargeFolderThresholdMb(value)
            textFromValue: function(value) { return qsTr("%1 MB").arg(value) }
            valueFromText: function(text) { return parseInt(text) }
            Layout.fillWidth: true
        }

        CheckBox {
            visible: root.controller.showExternalStorageConfirmation
            text: qsTr("Ask before syncing external storage")
            checked: root.controller.askBeforeExternalStorage
            font.pixelSize: Style.pixelSize + 2
            Accessible.role: Accessible.CheckBox
            Accessible.name: text
            onToggled: root.controller.setAskBeforeExternalStorage(checked)
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                primary: true
                text: qsTr("Done")
                onClicked: root.close()
            }
        }
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import Style
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

Dialog {
    id: root

    required property var searchModel

    property bool validationError: false

    anchors.centerIn: parent
    title: qsTr("Custom date range")
    modal: true
    onOpened: validationError = false

    footer: RowLayout {
        spacing: Style.wizardFooterSpacing

        Item {
            Layout.fillWidth: true
        }

        WizardButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }

        WizardButton {
            primary: true
            text: qsTr("Apply")
            onClicked: {
                root.validationError = !root.searchModel.setCustomDateRange(customSince.text, customUntil.text)
                if (!root.validationError) {
                    root.close()
                }
            }
        }
    }

    ColumnLayout {
        EnforcedPlainTextLabel {
            text: qsTr("Start date (YYYY-MM-DD)")
        }

        TextField {
            id: customSince

            Layout.fillWidth: true
            placeholderText: qsTr("YYYY-MM-DD")
        }

        EnforcedPlainTextLabel {
            text: qsTr("End date (YYYY-MM-DD)")
        }

        TextField {
            id: customUntil

            Layout.fillWidth: true
            placeholderText: qsTr("YYYY-MM-DD")
        }

        EnforcedPlainTextLabel {
            visible: root.validationError
            text: qsTr("Enter valid dates with the start date before the end date.")
            color: palette.accent
        }
    }
}

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Style
import "../../tray"

Pane {
    id: root

    required property var action
    readonly property bool validAction: action !== null && typeof action === "object"
        && typeof action.name === "string" && action.name.length > 0
        && action.args !== null && typeof action.args === "object" && !Array.isArray(action.args)
    readonly property var argumentNames: validAction ? Object.keys(action.args) : []

    padding: Style.standardSpacing

    background: Rectangle {
        color: Style.assistantActionCardBackground
        radius: Style.mediumRoundedButtonRadius
    }

    contentItem: ColumnLayout {
        spacing: Style.smallSpacing

        EnforcedPlainTextLabel {
            text: root.validAction ? root.action.name : qsTr("Action details are unavailable.")
            font.bold: true
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }

        Repeater {
            model: root.argumentNames
            delegate: EnforcedPlainTextLabel {
                required property string modelData
                readonly property var argumentValue: root.action.args[modelData]
                //: %1 is an action parameter name, %2 is its value, which may contain formatted JSON.
                text: qsTr("%1: %2").arg(modelData.replace(/_/g, " ")).arg(
                    typeof argumentValue === "object" ? JSON.stringify(argumentValue, null, 2) : String(argumentValue))
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
        }

        EnforcedPlainTextLabel {
            visible: root.validAction
            text: {
                switch (root.action?.impulse_radius) {
                case "self": return qsTr("These actions only affect you.")
                case "individuals": return qsTr("These actions affect specific other people.")
                case "group": return qsTr("These actions affect a group or team of people.")
                default: return qsTr("These actions may affect people or services outside this Nextcloud instance.")
                }
            }
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            visible: root.action?.destructive !== false
            text: qsTr("Some content may be lost.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}

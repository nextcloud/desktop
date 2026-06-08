/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import Style
import "../../tray"

Item {
    id: root

    required property var controller
    readonly property color primaryTextColor: Style.wizardPrimaryText
    readonly property color primaryButtonColor: Style.wizardPrimaryButtonBackground

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Item {
            Layout.fillHeight: true
        }

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "image://svgimage-custom-color/globe.svg/" + root.primaryButtonColor
            sourceSize.width: 72
            sourceSize.height: 72
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            fillMode: Image.PreserveAspectFit
        }

        EnforcedPlainTextLabel {
            text: qsTr("Switch to your browser")
            color: root.primaryTextColor
            font.pixelSize: Style.pixelSize + 8
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            id: activityIndicatorRow

            visible: root.controller.busy || root.controller.authPolling
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            NCBusyIndicator {
                running: activityIndicatorRow.visible
                visible: running
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
            }

            Item {
                Layout.fillWidth: true
            }
        }

        EnforcedPlainTextLabel {
            visible: root.controller.errorText !== ""
            text: root.controller.errorText
            color: Style.wizardErrorText
            font.pixelSize: Style.pixelSize + 1
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Item {
            Layout.fillHeight: true
        }
    }
}

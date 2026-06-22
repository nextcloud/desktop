/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Style
import "./tray"

Item {
    id: errorBox

    signal closeButtonClicked
    
    property string text: ""

    property color backgroundColor: Style.errorBoxBackgroundColor
    property bool showCloseButton: false
    
    implicitHeight: errorMessageLayout.implicitHeight + (2 * Style.standardSpacing)

    Rectangle {
        id: solidStripe

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        width: Style.errorBoxStripeWidth
        color: errorBox.backgroundColor
    }

    Rectangle {
        anchors.fill: parent
        color: errorBox.backgroundColor
        opacity: 0.2
    }

    GridLayout {
        id: errorMessageLayout

        anchors.fill: parent
        anchors.margins: Style.standardSpacing
        anchors.leftMargin: Style.standardSpacing + solidStripe.width

        columns: 2

        EnforcedPlainTextLabel {
            Layout.fillWidth: true
            font.bold: true
            text: qsTr("Error")
            visible: errorBox.showCloseButton
        }

        Button {
            Layout.preferredWidth: Style.iconButtonWidth
            Layout.preferredHeight: Style.iconButtonWidth

            background: null
            icon.color: palette.buttonText
            icon.source: "image://svgimage-custom-color/close.svg"

            visible: errorBox.showCloseButton
            enabled: visible

            onClicked: errorBox.closeButtonClicked()
        }

        EnforcedPlainTextLabel {
            id: errorMessage

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.columnSpan: 2

            wrapMode: Text.WordWrap
            text: errorBox.text
        }
    }
}

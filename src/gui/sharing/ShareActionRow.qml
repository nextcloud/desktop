/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Style
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

WizardItemDelegate {
    id: root

    required property string title
    required property string subtitle
    required property string actionIcon
    required property string actionName
    property bool actionEnabled: true

    signal actionRequested

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    contentItem: RowLayout {
        spacing: Style.standardSpacing

        Image {
            Layout.preferredWidth: Style.activityListButtonIconSize
            Layout.preferredHeight: Style.activityListButtonIconSize
            source: "image://svgimage-custom-color/share.svg/" + palette.buttonText
            sourceSize: Qt.size(Style.activityListButtonIconSize, Style.activityListButtonIconSize)
            fillMode: Image.PreserveAspectFit
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: root.title
                color: Style.wizardPrimaryText
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: root.subtitle
                color: Style.wizardSecondaryText
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }

        WizardButton {
            Layout.preferredWidth: implicitHeight
            leftPadding: 0
            rightPadding: 0
            text: ""
            iconSource: root.actionIcon
            enabled: root.actionEnabled

            Accessible.name: root.actionName
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name

            onClicked: root.actionRequested()
        }
    }
}
